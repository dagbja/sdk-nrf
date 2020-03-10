/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_tlv.h>
#include <lwm2m_acl.h>
#include <lwm2m_api.h>
#include <lwm2m_carrier.h>
#include <lwm2m_common.h>
#include <lwm2m_conn_mon.h>
#include <lwm2m_conn_stat.h>
#include <lwm2m_apn_conn_prof.h>
#include <lwm2m_portfolio.h>
#include <lwm2m_conn_ext.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_factory_bootstrap.h>
#include <lwm2m_device.h>
#include <lwm2m_firmware.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_firmware_download.h>
#include <lwm2m_remote.h>
#include <lwm2m_retry_delay.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_pdn.h>
#include <lwm2m_os.h>

#include <app_debug.h>
#include <at_interface.h>
#include <operator_check.h>
#include <nrf_socket.h>
#include <nrf_errno.h>
#include <sms_receive.h>

#include <sha256.h>

#define APP_USE_SOCKET_POLL             0 // Use socket poll() to check status
#define APP_ACL_DM_SERVER_HACK          1

#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     9998                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_LOCAL_CLIENT_PORT_OFFSET        9999                                            /**< Local port to connect to the LWM2M server. */

#define APP_SEC_TAG_OFFSET              25

#define APP_BOOTSTRAP_SEC_TAG           (APP_SEC_TAG_OFFSET + 0)                              /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_DIAGNOSTICS_SEC_TAG         (APP_SEC_TAG_OFFSET + 2)                              /**< Tag used to identify security credentials used by the client for diagnostics server. */

#define VZW_MANAGEMENT_INSTANCE_ID      1                                                   /**< Verizon Device Management server instance id. */
#define VZW_DIAGNOSTICS_INSTANCE_ID     2                                                   /**< Verizon Diagnostics server instance id. */
#define VZW_REPOSITORY_INSTANCE_ID      3                                                   /**< Verizon Data Repository server instance id. */

#define APP_NET_REG_STAT_HOME           1                                                   /**< Registered to home network. */
#define APP_NET_REG_STAT_SEARCHING      2                                                   /**< Searching an operator to register. */
#define APP_NET_REG_STAT_ROAM           5                                                   /**< Registered to roaming network. */

#define APP_CLIENT_ID_LENGTH            128                                                 /**< Buffer size to store the Client ID. */

#define APP_APN_NAME_BUF_LENGTH         64                                                  /**< Buffer size to store APN name. */

#define SECONDS_TO_UPDATE_EARLY         4                                                   /**< Number of seconds to Update early. */

static char m_apn_name_buf[APP_APN_NAME_BUF_LENGTH];                                        /**< Buffer to store APN name. */

/* Initialize config with default values. */
static lwm2m_carrier_config_t              m_app_config;

static lwm2m_server_config_t               m_server_conf[1+LWM2M_MAX_SERVERS];                /**< Server configuration structure. */
static lwm2m_client_identity_t             m_client_id;                                       /**< Client ID structure to hold the client's UUID. */

// Objects
static lwm2m_object_t                      m_bootstrap_server;                                /**< Named object to be used as callback object when bootstrap is completed. */

static char m_bootstrap_object_alias_name[] = "bs";                                           /**< Name of the bootstrap complete object. */

static coap_transport_handle_t             m_lwm2m_transport[1+LWM2M_MAX_SERVERS];            /**< CoAP transport handles for the secure servers. Obtained on @coap_security_setup. */

static int   m_admin_pdn_handle = -1;                                                         /**< VZWADMIN PDN connection handle. */
static bool  m_use_admin_pdn[1+LWM2M_MAX_SERVERS] = { true, true, true, false };              /**< Use VZWADMIN PDN for connection. */

static volatile lwm2m_state_t m_app_state = LWM2M_STATE_BOOTING;                              /**< Application state. Should be one of @ref lwm2m_state_t. */
static volatile bool        m_did_bootstrap;

/**< Security instance currently handled. */
static volatile uint16_t    m_security_instance;

/* Map table for Security instance and Server instance. */
static uint16_t m_server_instance_map[1+LWM2M_MAX_SERVERS];

/** @brief 15 digits IMEI stored as a NULL-terminated String. */
static char m_imei[16];

/**
 * @brief Subscriber number (MSISDN) stored as a NULL-terminated String.
 * Number with max 15 digits. Length varies depending on the operator and country.
 * VZW allows only 10 digits.
 */
static char m_msisdn[16];

static uint32_t m_net_stat;

static uint32_t m_coap_con_interval = CONFIG_NRF_LWM2M_CARRIER_COAP_CON_INTERVAL;

/* Structures for timers */
static void *state_update_timer;

typedef enum
{
    LWM2M_REQUEST_NONE,
    LWM2M_REQUEST_UPDATE,
    LWM2M_REQUEST_DEREGISTER
} lwm2m_update_request_t;

struct connection_update_t {
    void *timer;
    uint16_t security_instance;
    lwm2m_update_request_t requested;
    bool reconnect;
};

static struct connection_update_t m_connection_update[1+LWM2M_MAX_SERVERS];
static bool m_use_client_holdoff_timer;
static bool m_registration_ready;

/* Resolved server addresses */
static nrf_sa_family_t m_family_type[1+LWM2M_MAX_SERVERS] = { NRF_AF_INET6, NRF_AF_INET6, NRF_AF_INET6, NRF_AF_INET6 };  /**< Current IP versions, start using IPv6. */
static struct nrf_sockaddr_in6 m_remote_server[1+LWM2M_MAX_SERVERS];                                  /**< Remote secure server address to connect to. */
static volatile uint32_t tick_count = 0;

static void app_server_disconnect(uint16_t security_instance);
static int app_provision_psk(int sec_tag, char * identity, uint8_t identity_len, char * psk, uint8_t psk_len);
static int app_provision_secret_keys(void);
static void app_disconnect(void);
static const char * app_uri_get(char * p_server_uri, uint16_t * p_port, bool * p_secure);

extern int cert_provision();

static int app_event_notify(uint32_t type, void * data)
{
    lwm2m_carrier_event_t event =
    {
        .type = type,
        .data = data
    };

    return lwm2m_carrier_event_handler(&event);
}

static int app_event_error(uint32_t error_code, int32_t error_value)
{
    lwm2m_carrier_event_error_t error_event = {
        .code = error_code,
        .value = error_value
    };

    return app_event_notify(LWM2M_CARRIER_EVENT_ERROR, &error_event);
}

static bool lwm2m_state_set(lwm2m_state_t app_state)
{
    // Do not allow state change if network state has changed.
    // This may have happened during a blocking socket operation, typically
    // connect(), and then we must abort any ongoing state changes.
    if ((m_app_state == LWM2M_STATE_REQUEST_CONNECT) ||
        (m_app_state == LWM2M_STATE_REQUEST_DISCONNECT))
    {
        return false;
    }

    m_app_state = app_state;

    return true;
}

static int app_init_and_connect(void)
{
    app_event_notify(LWM2M_CARRIER_EVENT_CONNECTING, NULL);

    int err = lwm2m_os_lte_link_up();

    if (err == 0) {
        app_event_notify(LWM2M_CARRIER_EVENT_CONNECTED, NULL);
    } else {
        app_event_error(LWM2M_CARRIER_ERROR_CONNECT_FAIL, err);
    }

    return err;
}

static int app_offline(void)
{
    app_event_notify(LWM2M_CARRIER_EVENT_DISCONNECTING, NULL);

    // Set state to DISCONNECTED to avoid detecting "no registered network"
    // when provisioning security keys.
    lwm2m_state_set(LWM2M_STATE_DISCONNECTED);

    int err = lwm2m_os_lte_link_down();

    if (err == 0) {
        app_event_notify(LWM2M_CARRIER_EVENT_DISCONNECTED, NULL);
    } else {
        app_event_error(LWM2M_CARRIER_ERROR_DISCONNECT_FAIL, err);
    }

    return err;
}

/** @brief Get the server instance which corresponds to the security instance. */
static uint16_t server_instance_get(uint16_t security_instance)
{
    if (security_instance >= 1+LWM2M_MAX_SERVERS)
    {
        LWM2M_ERR("Illegal security instance: %u", security_instance);
        return 0;
    }

    if (m_server_instance_map[security_instance] == UINT16_MAX)
    {
        LWM2M_ERR("Missing server instance for security instance: %u", security_instance);
        return 0;
    }

    return m_server_instance_map[security_instance];
}

static void server_instance_update_map(void)
{
    uint16_t short_server_id;

    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        short_server_id = lwm2m_security_short_server_id_get(i);
        m_server_instance_map[i] = UINT16_MAX;

        if (short_server_id == 0) {
            continue;
        }

        for (int j = 0; j < 1+LWM2M_MAX_SERVERS; j++) {
            if (short_server_id == lwm2m_server_short_server_id_get(j)) {
                lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(j);
                LWM2M_INF("  </0/%u>,</1/%u>,</2/%u>;ssid=%u", i, j, p_instance->acl.id, short_server_id);
                m_server_instance_map[i] = j;
            }
        }

        if (m_server_instance_map[i] == UINT16_MAX) {
            LWM2M_INF("  </0/%u>;ssid=%u", i, short_server_id);
        }
    }
}

static bool lwm2m_is_registration_ready(void)
{
    for (int i = 1; i < 1 + LWM2M_MAX_SERVERS; i++) {
        if ((m_connection_update[i].security_instance != 0) &&
            (m_connection_update[i].requested == LWM2M_REQUEST_UPDATE))
        {
            /* More registrations to come, not ready yet. */
            return false;
        }
    }

    return true;
}

static bool lwm2m_is_deregistration_done(void)
{
    for (int i = 0; i < 1 + LWM2M_MAX_SERVERS; i++) {
        if (lwm2m_server_registered_get(i)) {
            // Still having registered servers
            return false;
        }
    }

    return true;
}

static uint16_t lwm2m_security_instance_from_remote(struct nrf_sockaddr *p_remote, uint16_t *short_server_id)
{
    uint16_t security_instance = UINT16_MAX;

    if (p_remote == NULL)
    {
        // Nothing to handle
        // LWM2M_WRN("Remote address missing");
    }
    else if (lwm2m_remote_short_server_id_find(short_server_id, p_remote) != 0)
    {
        LWM2M_WRN("Remote address not found");
    }
    else
    {
        // Find the server instance for the short server ID.
        for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
            if (lwm2m_security_short_server_id_get(i) == *short_server_id) {
                security_instance = i;
                break;
            }
        }
    }

    if ((security_instance == UINT16_MAX) &&
        (*short_server_id != 0))
    {
        LWM2M_WRN("Server instance for short server ID not found: %d", *short_server_id);
    }

    return security_instance;
}

/** Functions available from shell access */

void lwm2m_request_link_up(void)
{
    switch (m_net_stat) {
    case APP_NET_REG_STAT_HOME:
    case APP_NET_REG_STAT_ROAM:
    case APP_NET_REG_STAT_SEARCHING:
        LWM2M_WRN("Unexpected net state %d on link up", m_net_stat);
        break;
    default:
        m_app_state = LWM2M_STATE_REQUEST_LINK_UP;
        break;
    }
}

void lwm2m_request_link_down(void)
{
    switch (m_net_stat) {
    case APP_NET_REG_STAT_HOME:
    case APP_NET_REG_STAT_ROAM:
    case APP_NET_REG_STAT_SEARCHING:
        m_app_state = LWM2M_STATE_REQUEST_LINK_DOWN;
        break;
    default:
        LWM2M_WRN("Unexpected net state %d on link down", m_net_stat);
        break;
    }
}

void lwm2m_request_bootstrap(void)
{
    if ((m_app_state == LWM2M_STATE_IDLE) ||
        (m_app_state == LWM2M_STATE_DISCONNECTED))
    {
        app_disconnect();
        lwm2m_bootstrap_clear();
        lwm2m_request_connect();
    }
}

void lwm2m_request_connect(void)
{
    // Request connect only if not in a connect retry wait
    if ((m_app_state != LWM2M_STATE_BS_CONNECT_RETRY_WAIT) &&
        (m_app_state != LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT))
    {
        m_app_state = LWM2M_STATE_REQUEST_CONNECT;
    }
}

void lwm2m_request_server_update(uint16_t security_instance, bool reconnect)
{
    if (m_lwm2m_transport[security_instance] != -1 || reconnect) {
        m_connection_update[security_instance].requested = LWM2M_REQUEST_UPDATE;
    }
}

void lwm2m_request_deregister(void)
{
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (lwm2m_server_registered_get(i) && m_lwm2m_transport[i] != -1) {
            m_connection_update[i].requested = LWM2M_REQUEST_DEREGISTER;
        }
    }
}

void lwm2m_request_disconnect(void)
{
    // Only request disconnect if not already disconnected
    if (m_app_state != LWM2M_STATE_DISCONNECTED) {
        m_app_state = LWM2M_STATE_REQUEST_DISCONNECT;
    }
}

void lwm2m_request_reset(void)
{
    m_app_state = LWM2M_STATE_RESET;
}

lwm2m_state_t lwm2m_state_get(void)
{
    return m_app_state;
}

char *lwm2m_client_id_get(uint16_t *p_len)
{
    *p_len = m_client_id.len;
    return (char *)&m_client_id.value;
}

char *lwm2m_imei_get(void)
{
    return m_imei;
}

/** Return MSISDN. For VzW this must be exactly 10 digits. */
char *lwm2m_msisdn_get(void)
{
    char * p_msisdn = m_msisdn;

    if (strlen(p_msisdn) == 0) {
        // MSISDN has not been read from SIM yet. We need to be connected first.
        return p_msisdn;
    }

    if (operator_is_vzw(false))
    {
        // MSISDN is read from Modem and includes country code.
        // The country code "+1" should not be used in VZW network.
        p_msisdn = &m_msisdn[2]; // Remove country code "+1".
    }
    else if (operator_is_vzw(true))
    {
        // Make sure the MSISDN value is 10 digits long.
        size_t len = strlen(m_msisdn);
        __ASSERT_NO_MSG(len >= 10);
        p_msisdn = &m_msisdn[len - 10];
    }

    if (operator_is_vzw(true))
    {
        // MSISDN is used to generate the Client ID. Must be 10 digits in VZW.
        __ASSERT(strlen(p_msisdn) == 10, "Invalid MSISDN length");
    }

    return p_msisdn;
}

bool lwm2m_did_bootstrap(void)
{
    return m_did_bootstrap;
}

bool lwm2m_is_admin_pdn_ready()
{
    return (m_admin_pdn_handle != -1);
}

uint16_t lwm2m_security_instance(void)
{
    return m_security_instance;
}

int64_t lwm2m_coap_con_interval_get(void)
{
    return m_coap_con_interval;
}

void lwm2m_coap_con_interval_set(int64_t con_interval)
{
    m_coap_con_interval = con_interval;
}

nrf_sa_family_t lwm2m_family_type_get(uint16_t security_instance)
{
    return m_family_type[security_instance];
}

int32_t lwm2m_state_update_delay(void)
{
    return lwm2m_os_timer_remaining(state_update_timer);
}

void lwm2m_system_shutdown(void)
{
    app_disconnect();

    lwm2m_os_lte_power_down();
    lwm2m_os_bsdlib_shutdown();

    m_app_state = LWM2M_STATE_SHUTDOWN;

    LWM2M_INF("LTE link down");
}

void lwm2m_system_reset(bool force_reset)
{
    int ret = app_event_notify(LWM2M_CARRIER_EVENT_REBOOT, NULL);

    if (ret == 0 || force_reset)
    {
        if (m_app_state != LWM2M_STATE_SHUTDOWN) {
            lwm2m_system_shutdown();
        }

        lwm2m_os_sys_reset();
    }
    else
    {
        LWM2M_INF("Reboot deferred by application");
    }
}

/* Read the access point name into a buffer, and null-terminate it.
 * Returns the length of the access point name.
 */
static int admin_apn_get(char *buf, size_t len)
{
    char *apn_name;
    uint8_t read = 0;

    apn_name = lwm2m_conn_mon_class_apn_get(2, &read);
    if (len < read + 1) {
        return -1;
    }

    memcpy(buf, apn_name, read);
    buf[read] = '\0';

    return read;
}

/**@brief Setup ADMIN PDN connection, if necessary */
int lwm2m_admin_pdn_activate(uint16_t security_instance)
{
    int rc;

    if (!operator_is_vzw(false) ||
        !m_use_admin_pdn[security_instance]) {
        /* Nothing to do */
        lwm2m_retry_delay_pdn_reset();
        return 0;
    }

    admin_apn_get(m_apn_name_buf, sizeof(m_apn_name_buf));
    LWM2M_INF("PDN setup: %s", lwm2m_os_log_strdup(m_apn_name_buf));

    /* Register for packet domain events before activating ADMIN PDN */
    at_apn_register_for_packet_events();

    rc = lwm2m_pdn_activate(&m_admin_pdn_handle, m_apn_name_buf);
    if (rc < 0) {
        at_apn_unregister_from_packet_events();

        return lwm2m_retry_delay_pdn_get();
    }

    /* PDN was active */
    if (rc == 0) {
        at_apn_unregister_from_packet_events();
        lwm2m_retry_delay_pdn_reset();
        return 0;
    }

    LWM2M_INF("Activating %s", lwm2m_os_log_strdup(m_apn_name_buf));

    /* PDN was reactived, wait for IPv6 */
    rc = at_apn_setup_wait_for_ipv6(&m_admin_pdn_handle, m_apn_name_buf);

    /* Unregister from packet domain events after waiting for IPv6 */
    at_apn_unregister_from_packet_events();

    if (rc) {
        return lwm2m_retry_delay_pdn_get();
    }

    lwm2m_retry_delay_pdn_reset();

    return 0;
}

/**@brief Disconnect ADMIN PDN connection. */
static void lwm2m_admin_pdn_deactivate(void)
{
    if (m_admin_pdn_handle != -1)
    {
        nrf_close(m_admin_pdn_handle);
        m_admin_pdn_handle = -1;
    }
}

bool lwm2m_request_remote_reconnect(struct nrf_sockaddr *p_remote)
{
    bool requested = false;

    uint16_t short_server_id = 0;
    uint16_t security_instance = lwm2m_security_instance_from_remote(p_remote, &short_server_id);;

    // Reconnect if not already in the connect/register phase for this server.
    if ((m_app_state == LWM2M_STATE_IDLE) ||
        (security_instance != m_security_instance))
    {
        // Only reconnect if remote is found and already connected.
        if ((security_instance != UINT16_MAX) &&
            (m_lwm2m_transport[security_instance] != -1))
        {
            app_server_disconnect(security_instance);
            lwm2m_request_server_update(security_instance, true);
            lwm2m_remote_reconnecting_set(short_server_id);

            requested = true;
        }
    }

    return requested;
}

static void app_vzw_sha256_psk(char *p_imei, uint16_t short_server_id, char *p_psk)
{
    SHA256_CTX ctx;
    char imei_and_id[24];

    // VZW PSK Secret Key Algorithm: sha256sum(imei+short_server_id)
    snprintf(imei_and_id, sizeof(imei_and_id), "%s%3d", p_imei, short_server_id);

    sha256_init(&ctx);
    sha256_update(&ctx, imei_and_id, strlen(imei_and_id));
    sha256_final(&ctx, p_psk);
}

/**
 * @brief Read ICCID and MSISDN from SIM.
 */
static int app_read_sim_values(void)
{
    // Read ICCID.
    char iccid[20];
    uint32_t len = sizeof(iccid);
    int ret = at_read_sim_iccid(iccid, &len);

    if (ret != 0) {
        LWM2M_ERR("No SIM ICCID available");
        return EACCES;
    }

    // Update Device object with current ICCID.
    lwm2m_device_set_sim_iccid(iccid, len);

    // Read MSISDN.
    ret = at_read_msisdn(m_msisdn, sizeof(m_msisdn));

    if (ret != 0)
    {
        if (operator_is_vzw(false))
        {
            // MSISDN is mandatory on VZW network. Cannot continue.
            LWM2M_ERR("No MSISDN available, cannot generate client ID");
            return EACCES;
        }
        else if (operator_is_vzw(true))
        {
            // If no MSISDN is available, use part of IMEI to generate a unique Client ID.
            // This is not allowed on VZW network. Use for testing purposes only.
            memcpy(m_msisdn, &m_imei[5], 10);
            m_msisdn[10] = '\0';
        }
    }

    return 0;
}

static bool app_bootstrap_keys_exists(void)
{
    bool key_exists = false;
    uint8_t perm_flags;
    int err_code;

    err_code = lwm2m_os_sec_identity_exists(APP_BOOTSTRAP_SEC_TAG,
                                            &key_exists, &perm_flags);

    if (err_code != 0) {
        LWM2M_ERR("Unable to check if bootstrap Identity exists (%d)", err_code);
        return false;
    }

    if (key_exists) {
        err_code = lwm2m_os_sec_psk_exists(APP_BOOTSTRAP_SEC_TAG,
                                           &key_exists, &perm_flags);

        if (err_code != 0) {
            LWM2M_ERR("Unable to check if bootstrap PSK exists (%d)", err_code);
            return false;
        }
    }

    return key_exists;
}

/**
 * @brief Generate a unique Client ID using device IMEI and MSISDN if available.
 * Factory reset to start bootstrap if MSISDN is different than last start.
 */
static int app_generate_client_id(void)
{
    char client_id[APP_CLIENT_ID_LENGTH];
    char last_used_msisdn[16];
    bool clear_bootstrap = false;
    bool provision_bs_psk = false;

    // Read SIM values, this may have changed since last LTE connect.
    int ret = app_read_sim_values();

    if (ret != 0) {
        return ret;
    }

    uint32_t last_used_operator_id = OPERATOR_ID_UNSET;
    int32_t len = lwm2m_last_used_operator_id_get(&last_used_operator_id);

    if (last_used_operator_id != operator_id(true)) {
        if (last_used_operator_id == OPERATOR_ID_UNSET) {
            LWM2M_INF("Carrier detected: %s",
                      lwm2m_os_log_strdup(operator_id_string(operator_id(true))));
        } else {
            LWM2M_INF("Carrier change detected: %s -> %s",
                      lwm2m_os_log_strdup(operator_id_string(last_used_operator_id)),
                      lwm2m_os_log_strdup(operator_id_string(operator_id(true))));
        }
        if (lwm2m_factory_bootstrap_update(&m_app_config)) {
            lwm2m_last_used_msisdn_set("", 0);
            clear_bootstrap = true;
        }

        lwm2m_last_used_operator_id_set(operator_id(true));
    }

    if (!app_bootstrap_keys_exists()) {
        provision_bs_psk = true;
    }

    if (operator_is_vzw(true)) {
        // Get the MSISDN with correct format for VZW.
        char * p_msisdn = lwm2m_msisdn_get();

        len = lwm2m_last_used_msisdn_get(last_used_msisdn, sizeof(last_used_msisdn));
        if (len > 0) {
            if (strlen(p_msisdn) > 0 && strcmp(p_msisdn, last_used_msisdn) != 0) {
                // MSISDN has changed, factory reset and initiate bootstrap.
                LWM2M_INF("New MSISDN detected: %s -> %s", lwm2m_os_log_strdup(last_used_msisdn), lwm2m_os_log_strdup(p_msisdn));
                lwm2m_last_used_msisdn_set(p_msisdn, strlen(p_msisdn));
                clear_bootstrap = true;
            }
        } else {
            lwm2m_last_used_msisdn_set(p_msisdn, strlen(p_msisdn));
            provision_bs_psk = true;
        }

        // Generate a unique Client ID based on IMEI and MSISDN.
        snprintf(client_id, sizeof(client_id), "urn:imei-msisdn:%s-%s", lwm2m_imei_get(), p_msisdn);
        memcpy(&m_client_id.value.imei_msisdn[0], client_id, strlen(client_id));
        m_client_id.len  = strlen(client_id);
        m_client_id.type = LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN;
    } else {
        // Generate a unique Client ID based on IMEI.
        snprintf(client_id, sizeof(client_id), "urn:imei:%s", lwm2m_imei_get());
        memcpy(&m_client_id.value.imei[0], client_id, strlen(client_id));
        m_client_id.len  = strlen(client_id);
        m_client_id.type = LWM2M_CLIENT_ID_TYPE_IMEI;
    }
    LWM2M_INF("Client ID: %s", lwm2m_os_log_strdup(client_id));

    if (clear_bootstrap) {
        lwm2m_bootstrap_clear();
        lwm2m_retry_delay_connect_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);
        provision_bs_psk = true;
    }

    if (provision_bs_psk) {
        int err = app_offline();
        if (err != 0) {
            return err;
        }

        if (provision_bs_psk) {
            ret = app_provision_psk(APP_BOOTSTRAP_SEC_TAG, client_id, strlen(client_id),
                                    m_app_config.psk, m_app_config.psk_length);
        }
        if (operator_is_vzw(true)) {
            char app_diagnostics_psk[SHA256_BLOCK_SIZE];
            app_vzw_sha256_psk(m_imei, 101, app_diagnostics_psk);
            ret = app_provision_psk(APP_DIAGNOSTICS_SEC_TAG, m_imei, strlen(m_imei),
                                    app_diagnostics_psk, sizeof(app_diagnostics_psk));
        }

        if (ret != 0) {
            app_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, ret);
        }

        err = app_init_and_connect();
        if (ret == 0 && err != 0) {
            // Only set ret from app_init_and_connect() if not already set from a failing app_provision_psk().
            ret = err;
        }
    }

    return ret;
}

/**@brief Delete all Security and Server instances.
 *
 * @param[in]  delete_bootstrap  Set if deleting bootstrap instances.
 */
static void delete_security_and_server_instances(bool delete_bootstrap)
{
    uint16_t bootstrap_ssid = lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID);

    // Delete all instances except Bootstrap server
    for (uint32_t i = 0; i < 1 + LWM2M_MAX_SERVERS; i++)
    {
        if (delete_bootstrap ||
            (i != LWM2M_BOOTSTRAP_INSTANCE_ID))
        {
            lwm2m_instance_storage_security_delete(i);
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)lwm2m_security_get_instance(i));
        }

        if (delete_bootstrap ||
            (lwm2m_server_short_server_id_get(i) != bootstrap_ssid))
        {
            lwm2m_instance_storage_server_delete(i);
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)lwm2m_server_get_instance(i));
        }
    }
}

/**@brief Application implementation of the root handler interface.
 *
 * @details This function is not bound to any object or instance. It will be called from
 *          LWM2M upon an action on the root "/" URI path. During bootstrap it is expected
 *          to get a DELETE operation on this URI.
 */
uint32_t lwm2m_coap_handler_root(uint8_t op_code, coap_message_t * p_request)
{
    delete_security_and_server_instances(false);

    (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);

    return 0;
}

const void * lwm2m_observable_reference_get(const uint16_t *p_path, uint8_t path_len, uint8_t *p_type)
{
    const void * value = NULL;
    lwm2m_object_t *object;
    lwm2m_instance_t *instance;
    int ret;

    if (p_path == NULL || path_len <= 0 || p_type == NULL)
    {
        return value;
    }

    if (path_len == 1)
    {
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
        ret = lwm2m_lookup_object(&object, p_path[0]);
        if (ret != 0)
        {
            return NULL;
        }
        else
        {
            return object;
        }
    }

    if (path_len == 2)
    {
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
        ret = lwm2m_lookup_instance(&instance, p_path[0], p_path[1]);
        if (ret != 0)
        {
            return NULL;
        }
        else
        {
            return instance;
        }
    }

    switch (p_path[0])
    {
    case LWM2M_OBJ_DEVICE:
        value = lwm2m_device_resource_reference_get(p_path[2], p_type);
        break;
    case LWM2M_OBJ_CONN_MON:
        value = lwm2m_conn_mon_resource_reference_get(p_path[2], p_type);
        break;
    case LWM2M_OBJ_FIRMWARE:
        value = lwm2m_firmware_resource_reference_get(p_path[2], p_type);
        break;
    case LWM2M_OBJ_SERVER:
        value = lwm2m_server_resource_reference_get(p_path[1], p_path[2], p_type);
        break;
    case LWM2M_OBJ_SECURITY:
    case LWM2M_OBJ_ACL:
    case LWM2M_OBJ_LOCATION:
    case LWM2M_OBJ_CONN_STAT:
        // Unsupported observables.
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
        break;
    default:
        // Unsupported observables.
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
        break;
    }

    return value;
}

void lwm2m_observer_notify_path(const uint16_t *p_path, uint8_t path_len, struct nrf_sockaddr *p_remote_server)
{
    // Currently, we only support observe on resource level.
    if (path_len < 3)
    {
        return;
    }
    // TODO: Assert p_path is not NULL.
    uint16_t object_id = p_path[0];
    uint16_t instance_id = p_path[1];
    uint16_t resource_id = p_path[2];
    switch (object_id)
    {
    case LWM2M_OBJ_DEVICE:
        lwm2m_device_notify_resource(p_remote_server, resource_id);
        break;
    case LWM2M_OBJ_CONN_MON:
        lwm2m_conn_mon_notify_resource(p_remote_server, resource_id);
        break;
    case LWM2M_OBJ_FIRMWARE:
        lwm2m_firmware_notify_resource(p_remote_server, resource_id);
        break;
    case LWM2M_OBJ_SERVER:
        lwm2m_server_notify_resource(p_remote_server, instance_id, resource_id);
        break;
    case LWM2M_OBJ_SECURITY:
    case LWM2M_OBJ_ACL:
    case LWM2M_OBJ_LOCATION:
    case LWM2M_OBJ_CONN_STAT:
        // Unsupported observables.
        break;
    default:
        break;
    }
}

static void lwm2m_notif_attribute_default_value_set(uint8_t type, void * p_value, struct nrf_sockaddr *p_remote_server)
{
    // TODO: Assert value != NULL && p_remote_server != NULL
    uint16_t server_id;

    lwm2m_remote_short_server_id_find(&server_id, p_remote_server);
    server_id = lwm2m_server_short_server_id_get(server_id);

    switch (type)
    {
    case LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD:
        *(int32_t *)p_value = lwm2m_server_get_instance(server_id)->default_minimum_period;
        break;
    case LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD:
        *(int32_t *)p_value = lwm2m_server_get_instance(server_id)->default_maximum_period;
        break;
    case LWM2M_ATTRIBUTE_TYPE_GREATER_THAN:
        *(int32_t *)p_value = INT32_MAX;
        break;
    case LWM2M_ATTRIBUTE_TYPE_LESS_THAN:
        *(int32_t *)p_value = -INT32_MAX;
        break;
    case LWM2M_ATTRIBUTE_TYPE_STEP:
        *(int32_t *)p_value = INT32_MAX;
        break;
    default:
        // TODO: Assert for unsupported type.
        break;
    }
}

static void app_init_sockaddr_in(struct nrf_sockaddr *addr, nrf_sa_family_t ai_family, uint16_t port)
{
    memset(addr, 0, sizeof(struct nrf_sockaddr_in6));

    if (ai_family == NRF_AF_INET)
    {
        struct nrf_sockaddr_in *addr_in = (struct nrf_sockaddr_in *)addr;

        addr_in->sin_len = sizeof(struct nrf_sockaddr_in);
        addr_in->sin_family = ai_family;
        addr_in->sin_port = nrf_htons(port);
    }
    else
    {
        struct nrf_sockaddr_in6 *addr_in6 = (struct nrf_sockaddr_in6 *)addr;

        addr_in6->sin6_len = sizeof(struct nrf_sockaddr_in6);
        addr_in6->sin6_family = ai_family;
        addr_in6->sin6_port = nrf_htons(port);
    }
}

static const char * app_uri_get(char * server_uri, uint16_t * p_port, bool * p_secure) {
    const char *hostname;

    if (strncmp(server_uri, "coaps://", 8) == 0) {
        hostname = &server_uri[8];
        *p_port = 5684;
        *p_secure = true;
    } else if (strncmp(server_uri, "coap://", 7) == 0) {
        hostname = &server_uri[7];
        *p_port = 5683;
        *p_secure = false;
    } else {
        LWM2M_ERR("Invalid server URI: %s", lwm2m_os_log_strdup(server_uri));
        return NULL;
    }

    char *sep = strchr(hostname, ':');
    if (sep) {
        *sep = '\0';
        *p_port = atoi(sep + 1);
    }

    return hostname;
}

static void app_printable_ip_address(struct nrf_sockaddr * addr, char * ip_buffer, size_t ip_buffer_len)
{
    switch (addr->sa_family) {
    case NRF_AF_INET:
    {
        uint32_t val = ((struct nrf_sockaddr_in *)addr)->sin_addr.s_addr;
        snprintf(ip_buffer, ip_buffer_len, "%u.%u.%u.%u",
                ((uint8_t *)&val)[0], ((uint8_t *)&val)[1], ((uint8_t *)&val)[2], ((uint8_t *)&val)[3]);
        break;
    }

    case NRF_AF_INET6:
    {
        size_t pos = 0;
        bool elided = false;

        // Poor man's elided IPv6 address print.
        for (uint8_t i = 0; i < 16; i += 2) {
            uint16_t val = (((struct nrf_sockaddr_in6 *)addr)->sin6_addr.s6_addr[i] << 8) +
                           (((struct nrf_sockaddr_in6 *)addr)->sin6_addr.s6_addr[i+1]);

            if (elided || val != 0) {
                if (pos >= 2 && ip_buffer[pos-2] == ':' && ip_buffer[pos-1] == ':') {
                    elided = true;
                }
                pos += snprintf(&ip_buffer[pos], ip_buffer_len - pos, "%x", val);
            }

            if (pos >= 2 && (ip_buffer[pos-2] != ':' || ip_buffer[pos-1] != ':')) {
                pos += snprintf(&ip_buffer[pos], ip_buffer_len - pos, ":");
            }
        }

        if (pos >= 1 && ip_buffer[pos-1] == ':') {
            // Remove trailing ':'
            ip_buffer[pos-1] = '\0';
        }
        break;
    }

    default:
        snprintf(ip_buffer, ip_buffer_len, "Unknown family: %d", addr->sa_family);
        break;
    }
}

static uint32_t app_resolve_server_uri(char                * server_uri,
                                       uint8_t               uri_len,
                                       struct nrf_sockaddr * addr,
                                       bool                * secure,
                                       nrf_sa_family_t       family_type,
                                       int                   pdn_handle)
{
    // Create a string copy to null-terminate hostname within the server_uri.
    char * p_server_uri_val = lwm2m_os_malloc(uri_len + 1);
    strncpy(p_server_uri_val, server_uri, uri_len);
    p_server_uri_val[uri_len] = 0;

    uint16_t port;
    const char *hostname = app_uri_get(p_server_uri_val, &port, secure);

    if (hostname == NULL) {
        lwm2m_os_free(p_server_uri_val);
        return EINVAL;
    }

    struct nrf_addrinfo hints = {
        .ai_family = family_type,
        .ai_socktype = NRF_SOCK_DGRAM
    };

    // Structures that might be pointed to by APN hints.
    struct nrf_addrinfo apn_hints;

    if (pdn_handle > -1)
    {
        admin_apn_get(m_apn_name_buf, sizeof(m_apn_name_buf));

        apn_hints.ai_family    = NRF_AF_LTE;
        apn_hints.ai_socktype  = NRF_SOCK_MGMT;
        apn_hints.ai_protocol  = NRF_PROTO_PDN;
        apn_hints.ai_canonname = m_apn_name_buf;

        hints.ai_next = &apn_hints;
    }

    LWM2M_INF("Doing DNS lookup using %s (APN %s)",
            (family_type == NRF_AF_INET6) ? "IPv6" : "IPv4",
            (pdn_handle > -1) ? lwm2m_os_log_strdup(m_apn_name_buf) : "default");

    struct nrf_addrinfo *result;
    int ret_val = -1;
    int cnt = 1;

    // TODO:
    //  getaddrinfo() currently returns a mix of GAI error codes and NRF error codes.
    //  22 = NRF_EINVAL is invalid argument, but may also indicate no address found in the DNS query response.
    //  60 = NRF_ETIMEDOUT is a timeout waiting for DNS query response.
    //  50 = NRF_ENETDOWN is PDN down.
    while (ret_val != 0 && cnt <= 5) {
        ret_val = nrf_getaddrinfo(hostname, NULL, &hints, &result);
        if (ret_val != 0) {
            if(ret_val == NRF_EINVAL || ret_val == NRF_ETIMEDOUT || ret_val == NRF_ENETDOWN) {
                break;
            }
            lwm2m_os_sleep(1000 * cnt);
        }
        cnt++;
    }

    if (ret_val == NRF_EINVAL || ret_val == NRF_ETIMEDOUT) {
        LWM2M_WRN("No %s address found for \"%s\"", (family_type == NRF_AF_INET6) ? "IPv6" : "IPv4", lwm2m_os_log_strdup(hostname));
        lwm2m_os_free(p_server_uri_val);
        return EINVAL;
    } else if (ret_val == NRF_ENETDOWN) {
        LWM2M_ERR("Failed to lookup \"%s\": PDN down", lwm2m_os_log_strdup(hostname));
        lwm2m_os_free(p_server_uri_val);
        return ENETDOWN;
    } else if (ret_val != 0) {
        LWM2M_ERR("Failed to lookup \"%s\": %d", lwm2m_os_log_strdup(hostname), ret_val);
        lwm2m_os_free(p_server_uri_val);
        return ret_val;
    }

    app_init_sockaddr_in(addr, result->ai_family, port);

    if (result->ai_family == NRF_AF_INET) {
        ((struct nrf_sockaddr_in *)addr)->sin_addr.s_addr = ((struct nrf_sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
    } else {
        memcpy(((struct nrf_sockaddr_in6 *)addr)->sin6_addr.s6_addr, ((struct nrf_sockaddr_in6 *)result->ai_addr)->sin6_addr.s6_addr, 16);
    }

    nrf_freeaddrinfo(result);
    lwm2m_os_free(p_server_uri_val);

    char ip_buffer[64];
    app_printable_ip_address(addr, ip_buffer, sizeof(ip_buffer));
    LWM2M_INF("DNS result: %s", lwm2m_os_log_strdup(ip_buffer));

    return 0;
}

/**@brief Helper function to parse the uri and save the remote to the LWM2M remote database. */
static uint32_t app_lwm2m_parse_uri_and_save_remote(uint16_t              short_server_id,
                                                    char                * server_uri,
                                                    uint8_t               uri_len,
                                                    bool                * secure,
                                                    struct nrf_sockaddr * p_remote)
{
    uint32_t err_code;

    // Use DNS to lookup the IP
    err_code = app_resolve_server_uri(server_uri, uri_len, p_remote, secure, m_family_type[LWM2M_BOOTSTRAP_INSTANCE_ID], m_admin_pdn_handle);

    if (err_code == 0)
    {
        // Deregister the short_server_id in case it has been registered with a different address
        (void) lwm2m_remote_deregister(short_server_id);

        // Register the short_server_id
        err_code = lwm2m_remote_register(short_server_id, (struct nrf_sockaddr *)p_remote);
    }

    return err_code;
}

/**@brief Helper function to handle a connect retry. */
static void app_handle_connect_retry(uint16_t security_instance, bool fallback)
{
    bool start_retry_delay = true;

    if (fallback && !lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_IPv6) && !lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_FALLBACK))
    {
        // Fallback to the other IP version
        m_family_type[security_instance] = (m_family_type[security_instance] == NRF_AF_INET6) ? NRF_AF_INET : NRF_AF_INET6;

        if (m_family_type[security_instance] == NRF_AF_INET)
        {
            // No retry delay when IPv6 to IPv4 fallback
            LWM2M_INF("IPv6 to IPv4 fallback");
            start_retry_delay = false;
        }
    }

    if (start_retry_delay)
    {
        bool is_last = false;
        int32_t retry_delay = lwm2m_retry_delay_connect_get(security_instance, true, &is_last);

        if (retry_delay == -1) {
            LWM2M_ERR("Bootstrap procedure failed");
            m_app_state = LWM2M_STATE_DISCONNECTED;
            lwm2m_retry_delay_connect_reset(security_instance);

            app_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, 0);
            return;
        }

        if (is_last) {
            if (m_app_state == LWM2M_STATE_SERVER_REGISTER_WAIT) {
                // This is the last retry delay after no response from server.
                // Disconnect the session and retry on timeout.
                app_server_disconnect(security_instance);
            }

            app_event_notify(LWM2M_CARRIER_EVENT_DEFERRED, NULL);
        }

        LWM2M_INF("Retry delay for %ld minutes (server %u)", retry_delay / K_MINUTES(1), security_instance);
        lwm2m_os_timer_start(state_update_timer, retry_delay);
    } else {
        lwm2m_os_timer_start(state_update_timer, 0);
    }
}

static void app_set_bootstrap_if_last_retry_delay(uint16_t security_instance)
{
    if (operator_is_vzw(true) &&
        ((security_instance == VZW_MANAGEMENT_INSTANCE_ID) ||
         (security_instance == VZW_REPOSITORY_INSTANCE_ID)))
    {
        // Check if this is the last retry delay after an inability to establish a DTLS session.
        bool is_last = false;
        (void) lwm2m_retry_delay_connect_get(security_instance, false, &is_last);

        if (is_last) {
            // Repeat the bootstrap flow on timeout or reboot.
            LWM2M_INF("Last retry delay, trigger bootstrap on timeout");
            lwm2m_bootstrap_clear();
            lwm2m_state_set(LWM2M_STATE_BS_CONNECT_RETRY_WAIT);
        }
    }
}

static void app_restart_lifetime_timer(uint16_t security_instance)
{
    uint16_t server_instance = server_instance_get(security_instance);
    lwm2m_time_t lifetime = lwm2m_server_lifetime_get(server_instance);

    if (lifetime > SECONDS_TO_UPDATE_EARLY) {
        // Set timeout before lifetime expires to ensure we Update within the timeout
        lifetime -= SECONDS_TO_UPDATE_EARLY;
    }

    int32_t timeout = (int32_t)K_SECONDS(lifetime);
    if (timeout <= 0) {
        // FIXME: Lifetime timer too big for Zephyr, set to maximum possible value for now
        timeout = INT32_MAX;
    }

    m_connection_update[security_instance].reconnect = false;
    lwm2m_os_timer_start(m_connection_update[security_instance].timer, timeout);
}

static void app_cancel_lifetime_timer(uint16_t security_instance)
{
    lwm2m_os_timer_cancel(m_connection_update[security_instance].timer);
}

/**@brief LWM2M notification handler. */
void lwm2m_notification(lwm2m_notification_type_t   type,
                        struct nrf_sockaddr       * p_remote,
                        uint8_t                     coap_code,
                        uint32_t                    err_code)
{
#if defined(CONFIG_NRF_LWM2M_ENABLE_LOGS)
    static char *str_type[] = { "Bootstrap", "Register", "Update", "Deregister" };
#endif
    LWM2M_INF("Got LWM2M notification %s  CoAP %d.%02d  err:%lu", str_type[type], coap_code >> 5, coap_code & 0x1f, err_code);

    if ((m_app_state == LWM2M_STATE_REQUEST_DISCONNECT) ||
        (m_app_state == LWM2M_STATE_DISCONNECTED)) {
        // Disconnect requested or disconnected, ignore the notification
        return;
    }

    if (type == LWM2M_NOTIFCATION_TYPE_BOOTSTRAP)
    {
        if (coap_code == COAP_CODE_204_CHANGED)
        {
            if (lwm2m_state_set(LWM2M_STATE_BOOTSTRAPPING)) {
                LWM2M_INF("Bootstrap timeout set to 20 seconds");
                lwm2m_os_timer_start(state_update_timer, K_SECONDS(20));
            }
        }
        else if (coap_code == 0 || coap_code == COAP_CODE_403_FORBIDDEN)
        {
            // No response or received a 4.03 error.
            if (lwm2m_state_set(LWM2M_STATE_BOOTSTRAP_WAIT)) {
                app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
            }
        }
        else
        {
            LWM2M_ERR("Bootstrap procedure failed (%d.%02d)", coap_code >> 5, coap_code & 0x1f);
            m_app_state = LWM2M_STATE_DISCONNECTED;
            app_server_disconnect(LWM2M_BOOTSTRAP_INSTANCE_ID);
            lwm2m_retry_delay_connect_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);

            app_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, 0);
        }
        return;
    }

    uint16_t short_server_id = 0;
    uint16_t security_instance = lwm2m_security_instance_from_remote(p_remote, &short_server_id);
    uint16_t server_instance = server_instance_get(security_instance);

    if (security_instance == UINT16_MAX)
    {
        // Not found
        return;
    }

    if (type == LWM2M_NOTIFCATION_TYPE_REGISTER)
    {
        app_restart_lifetime_timer(security_instance);

        if (coap_code == COAP_CODE_201_CREATED || coap_code == COAP_CODE_204_CHANGED)
        {
            LWM2M_INF("Registered (server %u)", security_instance);

            lwm2m_retry_delay_connect_reset(security_instance);
            lwm2m_server_registered_set(server_instance, true);

            // Reset connection update in case this has been requested while connecting
            m_connection_update[security_instance].requested = LWM2M_REQUEST_NONE;

            lwm2m_state_set(LWM2M_STATE_IDLE);

            // Refresh stored server object, to also include is_connected status and registration ID.
            lwm2m_instance_storage_server_store(server_instance);

            lwm2m_notif_attr_storage_restore(short_server_id);

            if (!m_registration_ready && lwm2m_is_registration_ready()) {
                m_use_client_holdoff_timer = false;
                m_registration_ready = true;
                app_event_notify(LWM2M_CARRIER_EVENT_READY, NULL);
            }
        }
        else
        {
            // No response or received a 4.0x error.
            if (lwm2m_state_set(LWM2M_STATE_SERVER_REGISTER_WAIT)) {
                if (operator_is_vzw(true) &&
                    (security_instance == VZW_MANAGEMENT_INSTANCE_ID) &&
                    (coap_code == COAP_CODE_400_BAD_REQUEST))
                {
                    // Received 4.00 error from VzW DM server, use last defined retry delay.
                    int32_t retry_delay = lwm2m_retry_delay_connect_get(security_instance, false, NULL);

                    // VZW HACK: Loop until the current delay is 8 minutes. This will give the
                    // last retry delay (24 hours) in the next call to lwm2m_retry_delay_connect_get()
                    // in app_handle_connect_retry().
                    while (retry_delay != K_MINUTES(8)) {
                        // If not second to last then fetch next.
                        retry_delay = lwm2m_retry_delay_connect_get(security_instance, true, NULL);
                    }
                }

                app_handle_connect_retry(security_instance, false);
            }
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_UPDATE)
    {
        if (coap_code == 0) {
            // No response from update request
            LWM2M_INF("Update timeout, reconnect (server %d)", security_instance);
            app_server_disconnect(security_instance);
            lwm2m_request_server_update(security_instance, true);

            if (m_app_state == LWM2M_STATE_SERVER_REGISTER_WAIT) {
                // Timeout sending update after a server connect, reconnect from idle state.
                lwm2m_state_set(LWM2M_STATE_IDLE);
            }
        }
        else if ((coap_code == COAP_CODE_400_BAD_REQUEST) ||
                 (coap_code == COAP_CODE_403_FORBIDDEN) ||    // AT&T reports this when different DTLS session
                 (coap_code == COAP_CODE_404_NOT_FOUND))
        {
            // If not found, ignore.
            (void)lwm2m_remote_location_delete(short_server_id);
            lwm2m_server_registered_set(server_instance, false);
            lwm2m_instance_storage_server_store(server_instance);

            // Reset state to get back to registration.
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECTED);
        }
        else if (m_app_state == LWM2M_STATE_SERVER_REGISTER_WAIT)
        {
            // Update instead of register during connect
            LWM2M_INF("Updated after connect (server %d)", security_instance);
            lwm2m_retry_delay_connect_reset(security_instance);

            if (!m_registration_ready) {
                lwm2m_observer_storage_restore(short_server_id, m_lwm2m_transport[security_instance]);
                lwm2m_notif_attr_storage_restore(short_server_id);
            }

            // Reset connection update in case this has been requested while connecting
            m_connection_update[security_instance].requested = LWM2M_REQUEST_NONE;

            lwm2m_state_set(LWM2M_STATE_IDLE);

            if (!m_registration_ready && lwm2m_is_registration_ready()) {
                m_use_client_holdoff_timer = false;
                m_registration_ready = true;
                app_event_notify(LWM2M_CARRIER_EVENT_READY, NULL);
            }

            if (lwm2m_remote_reconnecting_get(short_server_id)) {
                coap_observer_t *p_observer = NULL;
                lwm2m_remote_reconnecting_clear(short_server_id);

                while (coap_observe_server_next_get(&p_observer, p_observer, NULL) == 0)
                {
                    if (memcmp(p_observer->remote, p_remote, sizeof(struct nrf_sockaddr)) == 0)
                    {
                        p_observer->transport = m_lwm2m_transport[security_instance];
                    }
                }
                lwm2m_observer_process(true);
            }
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_DEREGISTER)
    {
        // We have successfully deregistered.
        lwm2m_server_registered_set(server_instance, false);

        // Store server object to know that its not registered, and location should be cleared.
        lwm2m_instance_storage_server_store(server_instance);

        if (m_app_state == LWM2M_STATE_SERVER_DEREGISTERING)
        {
            LWM2M_INF("Deregistered (server %d)", security_instance);
            app_server_disconnect(security_instance);

            if (lwm2m_is_deregistration_done()) {
                m_app_state = LWM2M_STATE_DISCONNECTED;
            }
        }
        else
        {
            int32_t delay = (int32_t) lwm2m_server_disable_timeout_get(server_instance);
            LWM2M_INF("Disable [%ld seconds] (server %d)", delay, security_instance);
            app_server_disconnect(security_instance);

            m_connection_update[security_instance].reconnect = true;
            lwm2m_os_timer_start(m_connection_update[security_instance].timer, K_SECONDS(delay));
        }
    }
}

bool lwm2m_coap_error_handler(uint32_t error_code, coap_message_t * p_message)
{
    bool handled = false;
    // Handle CoAP failures when:
    // - sending error responses in send_error_response() fails
    // - decode error in coap_transport_read()
    // - send() error on retransmitting message in coap_time_tick()

    LWM2M_WRN("CoAP failure: %s (%ld), %s (%d)",
              lwm2m_os_log_strdup(strerror(error_code)), error_code,
              lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());

    // Handle error when not able to send on socket.
    if (error_code == EIO && lwm2m_os_errno() == NRF_EOPNOTSUPP) {
        handled = lwm2m_request_remote_reconnect(p_message->remote);
    }

    return handled;
}

uint32_t lwm2m_handler_error(uint16_t           short_server_id,
                             lwm2m_instance_t * p_instance,
                             coap_message_t   * p_request,
                             uint32_t           err_code)
{
    uint32_t retval = 0;

    // LWM2M will send an answer to the server based on the error code.
    switch (err_code)
    {
        case ENOENT:
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            break;

        case EPERM:
            (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
            break;

        case EINVAL:
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            break;

        default:
            // Pass error to lower layer which will send out INTERNAL_SERVER_ERROR.
            retval = err_code;
            break;
    }

    return retval;
}

/**@brief Handle server lifetime.
 */
static void app_connection_update(void *timer)
{
    struct connection_update_t * connection_update_p = NULL;

    /* Find owner of a timer. */
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (m_connection_update[i].timer == timer) {
            connection_update_p = &m_connection_update[i];
            break;
        }
    }

    if (connection_update_p == NULL) {
        LWM2M_ERR("Failed to find timer owner");
        return;
    }

    lwm2m_request_server_update(connection_update_p->security_instance, connection_update_p->reconnect);
}

static void app_init_connection_update(void)
{
    // Register all servers having a URI.
    for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++) {
        uint8_t uri_len = 0;

        (void)lwm2m_security_server_uri_get(i, &uri_len);
        if (uri_len > 0) {
            lwm2m_request_server_update(i, true);
            if (m_connection_update[i].timer == NULL) {
                m_connection_update[i].timer = lwm2m_os_timer_get(app_connection_update);
            }
            m_connection_update[i].security_instance = i;
        } else {
            if (m_connection_update[i].timer != NULL) {
                lwm2m_os_timer_release(m_connection_update[i].timer);
                m_connection_update[i].timer = NULL;
            }
            m_connection_update[i].security_instance = 0;
        }
        m_connection_update[i].reconnect = false;
    }
}

static void app_misc_data_set_bootstrapped(bool bootstrapped)
{
    lwm2m_instance_storage_misc_data_t misc_data = { 0 };
    lwm2m_instance_storage_misc_data_load(&misc_data);
    misc_data.bootstrapped = bootstrapped ? 1 : 0;
    lwm2m_instance_storage_misc_data_store(&misc_data);
}

/**@brief Callback function for the named bootstrap complete object. */
uint32_t bootstrap_object_callback(lwm2m_object_t * p_object,
                                   uint16_t         instance_id,
                                   uint8_t          op_code,
                                   coap_message_t * p_request)
{
    LWM2M_INF("Bootstrap done, timeout cancelled");
    lwm2m_os_timer_cancel(state_update_timer);

    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    lwm2m_os_sleep(10); // TODO: figure out why this is needed before closing the connection

    // Close connection to bootstrap server.
    app_server_disconnect(LWM2M_BOOTSTRAP_INSTANCE_ID);
    lwm2m_retry_delay_connect_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);

    if (app_provision_secret_keys() != 0) {
        lwm2m_state_set(LWM2M_STATE_DISCONNECTED);
        return 0;
    }

    lwm2m_security_bootstrapped_set(LWM2M_BOOTSTRAP_INSTANCE_ID, true);  // TODO: this should be set by bootstrap server when bootstrapped
    m_did_bootstrap = true;

    // Clean bootstrap.
    app_misc_data_set_bootstrapped(true);

    LWM2M_INF("Store bootstrap settings");
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (lwm2m_security_short_server_id_get(i) != 0) {
            lwm2m_instance_storage_security_store(i);
        }

        if (lwm2m_server_short_server_id_get(i) != 0) {
            lwm2m_instance_storage_server_store(i);
        }
    }

    server_instance_update_map();

    (void)app_event_notify(LWM2M_CARRIER_EVENT_BOOTSTRAPPED, NULL);

    return 0;
}

void lwm2m_bootstrap_clear(void)
{
    app_misc_data_set_bootstrapped(false);
    lwm2m_security_bootstrapped_set(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
}

void lwm2m_bootstrap_reset(void)
{
    if (lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID) == 0) {
        // Server object not loaded yet
        lwm2m_instance_storage_security_load(LWM2M_BOOTSTRAP_INSTANCE_ID);
    }

    if (lwm2m_security_bootstrapped_get(LWM2M_BOOTSTRAP_INSTANCE_ID)) {
        // Security object exists and bootstrap is done
        lwm2m_security_bootstrapped_set(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
        lwm2m_instance_storage_security_store(LWM2M_BOOTSTRAP_INSTANCE_ID);
    }

    app_misc_data_set_bootstrapped(false);
    delete_security_and_server_instances(false);
    lwm2m_factory_bootstrap_init();

    lwm2m_device_init_acl();
    lwm2m_conn_mon_init_acl();
    lwm2m_firmware_init_acl();
    lwm2m_conn_stat_init_acl();
    lwm2m_apn_conn_prof_init_acl();
    lwm2m_portfolio_init_acl();
    lwm2m_conn_ext_init_acl();

    lwm2m_device_update_carrier_specific_settings();

    for (uint32_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        lwm2m_observer_delete(i);
    }
}

void lwm2m_factory_reset(void)
{
    app_misc_data_set_bootstrapped(false);

    // Provision bootstrap PSK and diagnostic PSK at next startup
    lwm2m_last_used_msisdn_set("", 0);
    lwm2m_last_used_operator_id_set(OPERATOR_ID_UNSET);

    // Delete the observers from the storage
    lwm2m_observer_storage_delete_all();

    // Delete the notification attributes from the storage
    lwm2m_notif_attr_storage_delete_all();

    // Delete all Security and Server instances
    delete_security_and_server_instances(true);
}

/**@brief Initialize server ACLs in a specific order. */
static void app_server_acl_init(void)
{
#if APP_ACL_DM_SERVER_HACK
    // FIXME: Init ACL for DM server[1] first to get ACL /2/0 which is according to Verizon spec
    uint32_t acl_init_order[] = { 1, 0, 2, 3 };
    for (uint32_t k = 0; k < ARRAY_SIZE(acl_init_order); k++)
    {
        uint32_t i = acl_init_order[k];
#else
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
#endif
        lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(i);

        // Initialize ACL on the instance.
        // The owner (second parameter) is set to LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID.
        // This will grant the Bootstrap server full permission to this instance.
        (void)lwm2m_acl_permissions_init(p_instance, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
    }
}

static void app_load_flash_objects(void)
{
    LWM2M_INF("Load bootstrap settings");
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_storage_security_load(i);
        lwm2m_instance_storage_server_load(i);

        if (lwm2m_security_short_server_id_get(i) != 0)
        {
            lwm2m_coap_handler_instance_add((lwm2m_instance_t *)lwm2m_security_get_instance(i));
        }

        if (lwm2m_server_short_server_id_get(i) != 0)
        {
            lwm2m_coap_handler_instance_add((lwm2m_instance_t *)lwm2m_server_get_instance(i));
        }
    }

    server_instance_update_map();

    lwm2m_instance_storage_misc_data_t misc_data;
    int32_t result = lwm2m_instance_storage_misc_data_load(&misc_data);
    if (result == 0 && misc_data.bootstrapped)
    {
        lwm2m_security_bootstrapped_set(LWM2M_BOOTSTRAP_INSTANCE_ID, true);
    }
    else
    {
        // storage reports that bootstrap has not been done, continue with bootstrap.
        lwm2m_security_bootstrapped_set(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
    }
}

static void app_lwm2m_create_objects(void)
{
    // Init functions will check operator ID.
    operator_id_read();

    lwm2m_security_init();
    lwm2m_server_init();
    app_server_acl_init();

    // Initialize security, server and acl from flash.
    app_load_flash_objects();

    lwm2m_device_init();
    lwm2m_conn_mon_init();
    lwm2m_firmware_init();
    lwm2m_firmware_download_init();
    lwm2m_conn_stat_init();
    lwm2m_apn_conn_prof_init();
    lwm2m_portfolio_init();
    lwm2m_conn_ext_init();
}

/**@brief LWM2M initialization.
 *
 * @details The function will register all implemented base objects as well as initial registration
 *          of existing instances. If bootstrap is not performed, the registration to the server
 *          will use what is initialized in this function.
 */
static void app_lwm2m_setup(void)
{
    (void)lwm2m_init(lwm2m_os_malloc, lwm2m_os_free);
    (void)lwm2m_remote_init();
    (void)lwm2m_acl_init();

    m_bootstrap_server.object_id    = LWM2M_NAMED_OBJECT;
    m_bootstrap_server.callback     = bootstrap_object_callback;
    m_bootstrap_server.p_alias_name = m_bootstrap_object_alias_name;
    (void)lwm2m_coap_handler_object_add(&m_bootstrap_server);

    // Add security support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_security_get_object());

    // Add server support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_server_get_object());

    // Add device support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_device_get_object());

    // Add connectivity monitoring support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_mon_get_object());

    // Add firmware support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_firmware_get_object());

    // Add connectivity statistics support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_stat_get_object());

    // Add APN connection profile support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_apn_conn_prof_get_object());

    // Add portfolio support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_portfolio_get_object());

    // Add connectivity extension support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_ext_get_object());

    // Add observer notification support.
    lwm2m_observers_notify_path_cb_set(lwm2m_observer_notify_path);

    // Add callback to set default notification attribute values.
    lwm2m_notif_attribute_default_cb_set(lwm2m_notif_attribute_default_value_set);

    // Add callback to get pointers to observables.
    lwm2m_observable_reference_get_cb_set(lwm2m_observable_reference_get);

    // Add callback to get the uptime in milliseconds and initialize the timer.
    lwm2m_observable_uptime_cb_initialize(lwm2m_os_uptime_get);
}

static void app_connect(void)
{
    // First ensure all existing connections are disconnected.
    app_disconnect();

    // Read operator ID.
    operator_id_read();

    if ((m_net_stat == APP_NET_REG_STAT_HOME) && operator_is_supported(true))
    {
        LWM2M_INF("Registered to home network (%s)", lwm2m_os_log_strdup(operator_id_string(OPERATOR_ID_CURRENT)));
        if (operator_is_supported(false)) {
            lwm2m_sms_receiver_enable();
        }

        // Generate a unique Client ID.
        if (app_generate_client_id() != 0) {
            lwm2m_state_set(LWM2M_STATE_DISCONNECTED);
        } else if (lwm2m_security_bootstrapped_get(LWM2M_BOOTSTRAP_INSTANCE_ID)) {
            lwm2m_state_set(LWM2M_STATE_IDLE);
            app_init_connection_update();
        } else {
            int32_t hold_off_time = lwm2m_security_hold_off_timer_get(LWM2M_BOOTSTRAP_INSTANCE_ID);
            if (hold_off_time > 0) {
                if (lwm2m_state_set(LWM2M_STATE_BS_HOLD_OFF)) {
                    LWM2M_INF("Bootstrap hold off timer [%ld seconds]", hold_off_time);
                    lwm2m_os_timer_start(state_update_timer, K_SECONDS(hold_off_time));
                }
            } else {
                // No hold off timer
                lwm2m_state_set(LWM2M_STATE_BS_CONNECT);
            }
        }
    } else {
        LWM2M_INF("Waiting for home network");
        lwm2m_sms_receiver_disable();
    }
}

static void app_bootstrap_connect(void)
{
    uint32_t err_code;
    bool secure;

    int32_t pdn_retry_delay = lwm2m_admin_pdn_activate(LWM2M_BOOTSTRAP_INSTANCE_ID);
    if (pdn_retry_delay > 0) {
        // Setup ADMIN PDN connection failed, try again
        if (lwm2m_state_set(LWM2M_STATE_BS_CONNECT_RETRY_WAIT)) {
            LWM2M_INF("PDN retry delay for %ld seconds (server 0)", pdn_retry_delay / K_SECONDS(1));
            lwm2m_os_timer_start(state_update_timer, pdn_retry_delay);
        }
        return;
    }

    // Save the remote address of the bootstrap server.
    uint8_t uri_len = 0;
    char * p_server_uri = lwm2m_security_server_uri_get(LWM2M_BOOTSTRAP_INSTANCE_ID, &uri_len);

    if (uri_len == 0 || !p_server_uri) {
        LWM2M_ERR("No Bootstrap URI found");
        m_app_state = LWM2M_STATE_DISCONNECTED;
        lwm2m_retry_delay_connect_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);

        app_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, 0);
        return;
    }

    err_code = app_lwm2m_parse_uri_and_save_remote(LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID,
                                                   p_server_uri,
                                                   uri_len,
                                                   &secure,
                                                   (struct nrf_sockaddr *)&m_remote_server[LWM2M_BOOTSTRAP_INSTANCE_ID]);
    if (err_code != 0) {
        if (err_code == ENETDOWN) {
            // PDN disconnected, just return to come back activating it immediately
            return;
        }

        if (lwm2m_state_set(LWM2M_STATE_BS_CONNECT_RETRY_WAIT)) {
            if (err_code == EINVAL) {
                app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, true);
            } else {
                app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
            }
        }
        return;
    }

    if (secure == true)
    {
        LWM2M_TRC("SECURE session (bootstrap)");

        struct nrf_sockaddr_in6 local_addr;
        app_init_sockaddr_in((struct nrf_sockaddr *)&local_addr, m_remote_server[LWM2M_BOOTSTRAP_INSTANCE_ID].sin6_family, LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT);

        #define SEC_TAG_COUNT 1

        nrf_sec_tag_t sec_tag_list[SEC_TAG_COUNT] = {APP_BOOTSTRAP_SEC_TAG};

        coap_sec_config_t setting =
        {
            .role          = 0,    // 0 -> Client role
            .sec_tag_count = SEC_TAG_COUNT,
            .sec_tag_list  = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr         = (struct nrf_sockaddr *)&local_addr,
            .setting      = &setting,
            .protocol     = NRF_SPROTO_DTLS1v2
        };

        if (m_use_admin_pdn[LWM2M_BOOTSTRAP_INSTANCE_ID] && m_admin_pdn_handle != -1)
        {
            admin_apn_get(m_apn_name_buf, sizeof(m_apn_name_buf));
            local_port.interface = m_apn_name_buf;
        }

        LWM2M_INF("Setup secure DTLS session (server 0) (APN %s)",
                  (local_port.interface) ? lwm2m_os_log_strdup(m_apn_name_buf) : "default");

        err_code = coap_security_setup(&local_port, (struct nrf_sockaddr *)&m_remote_server[LWM2M_BOOTSTRAP_INSTANCE_ID]);

        if (err_code == 0)
        {
            LWM2M_INF("Connected");
            lwm2m_state_set(LWM2M_STATE_BS_CONNECTED);
            m_lwm2m_transport[LWM2M_BOOTSTRAP_INSTANCE_ID] = local_port.transport;
        }
        else if (err_code == EINPROGRESS)
        {
            lwm2m_state_set(LWM2M_STATE_BS_CONNECT_WAIT);
            m_lwm2m_transport[LWM2M_BOOTSTRAP_INSTANCE_ID] = local_port.transport;
        }
        else if (err_code == EIO && (lwm2m_os_errno() == NRF_ENETDOWN)) {
            LWM2M_INF("Connection failed (PDN down)");

            // Just return, so we come back setup PDN again
            return;
        }
        else
        {
            LWM2M_INF("Connection failed: %s (%ld), %s (%d)",
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());

            if (lwm2m_state_set(LWM2M_STATE_BS_CONNECT_RETRY_WAIT)) {
                // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
                if (err_code == EIO && (lwm2m_os_errno() == NRF_EINVAL ||
                                        lwm2m_os_errno() == NRF_EOPNOTSUPP ||
                                        lwm2m_os_errno() == NRF_ENETUNREACH)) {
                    app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, true);
                 } else {
                    app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
                }
            }
        }
    }
    else
    {
        LWM2M_TRC("NON-SECURE session (bootstrap)");
        lwm2m_state_set(LWM2M_STATE_BS_CONNECTED);
    }
}

static void app_bootstrap(void)
{
    lwm2m_bootstrap_reset();

    m_use_client_holdoff_timer = true;

    uint32_t err_code = lwm2m_bootstrap((struct nrf_sockaddr *)&m_remote_server[LWM2M_BOOTSTRAP_INSTANCE_ID],
                                        &m_client_id,
                                        m_lwm2m_transport[LWM2M_BOOTSTRAP_INSTANCE_ID]);
    if (err_code == 0)
    {
        lwm2m_state_set(LWM2M_STATE_BOOTSTRAP_REQUESTED);
    }
}

static void update_server_conf(uint16_t security_instance)
{
    uint16_t server_instance = server_instance_get(security_instance);

    m_server_conf[security_instance].lifetime = lwm2m_server_lifetime_get(server_instance);

    if (operator_is_att(false) &&
        (security_instance != LWM2M_BOOTSTRAP_INSTANCE_ID))
    {
        // For AT&T MSISDN is fetched from the connectivity extension object.
        uint8_t msisdn_len = 0;
        m_server_conf[security_instance].msisdn.p_val = lwm2m_conn_ext_msisdn_get(&msisdn_len);
        m_server_conf[security_instance].msisdn.len = msisdn_len;
    }
}

static void app_server_connect(uint16_t security_instance)
{
    uint32_t err_code;
    bool secure;

    int32_t pdn_retry_delay = lwm2m_admin_pdn_activate(security_instance);
    if (pdn_retry_delay > 0) {
        // Setup ADMIN PDN connection failed, try again
        if (lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT)) {
            LWM2M_INF("PDN retry delay for %ld seconds (server %u)", pdn_retry_delay / K_SECONDS(1), security_instance);
            lwm2m_os_timer_start(state_update_timer, pdn_retry_delay);
        }
        return;
    }

    // Initialize server configuration structure.
    memset(&m_server_conf[security_instance], 0, sizeof(lwm2m_server_config_t));

    if (operator_is_supported(false))
    {
        m_server_conf[security_instance].binding.p_val = "UQS";
        m_server_conf[security_instance].binding.len = 3;

        if (security_instance != LWM2M_BOOTSTRAP_INSTANCE_ID)
        {
            m_server_conf[security_instance].msisdn.p_val = lwm2m_msisdn_get();
            m_server_conf[security_instance].msisdn.len = strlen(lwm2m_msisdn_get());
        }
    }

    update_server_conf(security_instance);

    // Set the short server id of the server in the config.
    m_server_conf[security_instance].short_server_id = lwm2m_security_short_server_id_get(security_instance);

    // Deregister the short_server_id in case it has been registered with a different address
    (void) lwm2m_remote_deregister(m_server_conf[security_instance].short_server_id);

    uint8_t uri_len = 0;
    char * p_server_uri = lwm2m_security_server_uri_get(security_instance, &uri_len);

    err_code = app_resolve_server_uri(p_server_uri,
                                      uri_len,
                                      (struct nrf_sockaddr *)&m_remote_server[security_instance],
                                      &secure,
                                      m_family_type[security_instance],
                                      m_use_admin_pdn[security_instance] ? m_admin_pdn_handle : -1);
    if (err_code != 0)
    {
        if (err_code == ENETDOWN) {
            // PDN disconnected, just return to come back activating it immediately
            return;
        }

        if (lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT)) {
            if (err_code == EINVAL) {
                app_handle_connect_retry(security_instance, true);
            } else {
                app_handle_connect_retry(security_instance, false);
            }
        }
        return;
    }

    if (secure == true)
    {
        LWM2M_TRC("SECURE session (register)");

        // TODO: Check if this has to be static.
        struct nrf_sockaddr_in6 local_addr;
        app_init_sockaddr_in((struct nrf_sockaddr *)&local_addr, m_remote_server[security_instance].sin6_family, LWM2M_LOCAL_CLIENT_PORT_OFFSET + security_instance);

        #define SEC_TAG_COUNT 1

        nrf_sec_tag_t sec_tag_list[SEC_TAG_COUNT] = { APP_SEC_TAG_OFFSET + security_instance };

        coap_sec_config_t setting =
        {
            .role          = 0,    // 0 -> Client role
            .sec_tag_count = SEC_TAG_COUNT,
            .sec_tag_list  = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr         = (struct nrf_sockaddr *)&local_addr,
            .setting      = &setting,
            .protocol     = NRF_SPROTO_DTLS1v2
        };

        if (m_use_admin_pdn[security_instance] && m_admin_pdn_handle != -1)
        {
            admin_apn_get(m_apn_name_buf, sizeof(m_apn_name_buf));
            local_port.interface = m_apn_name_buf;
        }

        LWM2M_INF("Setup secure DTLS session (server %u) (APN %s)",
                  security_instance,
                  (local_port.interface) ? lwm2m_os_log_strdup(m_apn_name_buf) : "default");

        err_code = coap_security_setup(&local_port, (struct nrf_sockaddr *)&m_remote_server[security_instance]);

        if (err_code == 0)
        {
            LWM2M_INF("Connected");
            // Reset state to get back to registration.
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECTED);
            m_lwm2m_transport[security_instance] = local_port.transport;
        }
        else if (err_code == EINPROGRESS)
        {
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT_WAIT);
            m_lwm2m_transport[security_instance] = local_port.transport;
        }
        else if (err_code == EIO && (lwm2m_os_errno() == NRF_ENETDOWN))
        {
            LWM2M_INF("Connection failed (PDN down)");

            // Just return, so we come back setup PDN again
            return;
        }
        else
        {
            LWM2M_INF("Connection failed: %s (%ld), %s (%d)",
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());

            if (lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT)) {
                // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
                if (err_code == EIO && (lwm2m_os_errno() == NRF_EINVAL ||
                                        lwm2m_os_errno() == NRF_EOPNOTSUPP ||
                                        lwm2m_os_errno() == NRF_ENETUNREACH)) {
                    app_handle_connect_retry(security_instance, true);
                } else {
                    app_handle_connect_retry(security_instance, false);
                }

                if (lwm2m_os_errno() != NRF_ENETUNREACH) {
                    app_set_bootstrap_if_last_retry_delay(security_instance);
                }
            }
        }
    }
    else
    {
        LWM2M_TRC("NON-SECURE session (register)");
        lwm2m_state_set(LWM2M_STATE_SERVER_CONNECTED);
    }
}

static void app_server_register(uint16_t security_instance)
{
    uint32_t err_code;
    uint32_t link_format_string_len = 0;
    uint8_t * p_link_format_string = NULL;

    uint16_t short_server_id = lwm2m_security_short_server_id_get(security_instance);

    // Dry run the link format generation, to check how much memory that is needed.
    err_code = lwm2m_coap_handler_gen_link_format(LWM2M_INVALID_INSTANCE, short_server_id, NULL, (uint16_t *)&link_format_string_len);

    if (err_code == 0) {
        // Allocate the needed amount of memory.
        p_link_format_string = lwm2m_os_malloc(link_format_string_len);

        if (p_link_format_string == NULL) {
            err_code = ENOMEM;
        }
    }

    if (err_code == 0) {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(LWM2M_INVALID_INSTANCE, short_server_id, p_link_format_string, (uint16_t *)&link_format_string_len);
    }

    if (err_code == 0) {
        err_code = lwm2m_register((struct nrf_sockaddr *)&m_remote_server[security_instance],
                                  &m_client_id,
                                  &m_server_conf[security_instance],
                                  m_lwm2m_transport[security_instance],
                                  p_link_format_string,
                                  (uint16_t)link_format_string_len);
    }

    if (p_link_format_string) {
        lwm2m_os_free(p_link_format_string);
    }

    if (err_code == 0) {
        lwm2m_state_set(LWM2M_STATE_SERVER_REGISTER_WAIT);
    } else {
        LWM2M_INF("Register failed: %s (%d), %s (%d), reconnect (server %d)",
                    lwm2m_os_log_strdup(strerror(err_code)), err_code,
                    lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                    security_instance);

        app_server_disconnect(security_instance);

        if (lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT)) {
            app_handle_connect_retry(security_instance, false);
        }
    }
}

void app_server_update(uint16_t security_instance, bool connect_update)
{
    bool restart_lifetime_timer = true;

    if ((m_app_state == LWM2M_STATE_IDLE) ||
        (connect_update) ||
        (security_instance != m_security_instance))
    {
        uint32_t err_code;

        update_server_conf(security_instance);

        err_code = lwm2m_update((struct nrf_sockaddr *)&m_remote_server[security_instance],
                                &m_server_conf[security_instance],
                                m_lwm2m_transport[security_instance]);
        if (err_code != 0) {
            LWM2M_INF("Update failed: %s (%d), %s (%d), reconnect (server %d)",
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                      security_instance);

            app_server_disconnect(security_instance);
            lwm2m_request_server_update(security_instance, true);

            if (connect_update) {
                // Failed sending update after a server connect, reconnect from idle state.
                if (!lwm2m_state_set(LWM2M_STATE_IDLE)) {
                    restart_lifetime_timer = false;
                }
            }
        } else {
            if (connect_update) {
                // Update after a server connect, set next state.
                if (!lwm2m_state_set(LWM2M_STATE_SERVER_REGISTER_WAIT)) {
                    restart_lifetime_timer = false;
                }
            }
        }
    }
    else
    {
        LWM2M_WRN("Unable to do server update (server %u)", security_instance);
    }

    if (restart_lifetime_timer) {
        app_restart_lifetime_timer(security_instance);
    }
}


static void app_remove_observers_on_deregister(uint16_t security_instance)
{
    uint32_t err_code;
    coap_observer_t *p_observer = NULL;

    while (coap_observe_server_next_get(&p_observer, p_observer, NULL) == 0)
    {
        if (memcmp(p_observer->remote, (struct nrf_sockaddr *)&m_remote_server[security_instance], sizeof(struct nrf_sockaddr)) == 0)
        {
            err_code = lwm2m_observe_unregister(p_observer->remote, p_observer->resource_of_interest);

            if (err_code != 0)
            {
                LWM2M_ERR("Removing observer after deregister failed: %s (%d), %s (%d) (server %d)",
                          lwm2m_os_log_strdup(strerror(err_code)), err_code,
                          lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                          security_instance);
            }

            err_code = lwm2m_observer_storage_delete(p_observer);

            if (err_code != 0)
            {
                LWM2M_ERR("Removing observer from flash failed: %s (%d), %s (%d) (server %d)",
                          lwm2m_os_log_strdup(strerror(err_code)), err_code,
                          lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                          security_instance);
            }
        }
    }
}

void app_server_disable(uint16_t security_instance)
{
    uint32_t err_code;

    app_cancel_lifetime_timer(security_instance);

    err_code = lwm2m_deregister((struct nrf_sockaddr *)&m_remote_server[security_instance],
                                m_lwm2m_transport[security_instance]);

    if (err_code != 0) {
        LWM2M_ERR("Disable failed: %s (%d), %s (%d) (server %d)",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  security_instance);
    }
}

static void app_server_deregister(uint16_t security_instance)
{
    uint32_t err_code;

    app_cancel_lifetime_timer(security_instance);

    err_code = lwm2m_deregister((struct nrf_sockaddr *)&m_remote_server[security_instance],
                                m_lwm2m_transport[security_instance]);

    if (err_code != 0) {
        LWM2M_ERR("Deregister failed: %s (%d), %s (%d) (server %d)",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  security_instance);

       return;
    }
    else
    {
        app_remove_observers_on_deregister(security_instance);
    }

    lwm2m_state_set(LWM2M_STATE_SERVER_DEREGISTERING);
}

static void app_server_disconnect(uint16_t security_instance)
{
    if (m_lwm2m_transport[security_instance] != -1) {
        app_cancel_lifetime_timer(security_instance);
        coap_security_destroy(m_lwm2m_transport[security_instance]);
        m_lwm2m_transport[security_instance] = -1;
    }
}

static void app_disconnect(void)
{
    // Destroy the secure session if any.
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        app_server_disconnect(i);
    }

    m_app_state = LWM2M_STATE_DISCONNECTED;
}

static void app_wait_state_update(void *timer)
{
    ARG_UNUSED(timer);

    switch (m_app_state)
    {
        case LWM2M_STATE_BS_HOLD_OFF:
            // Bootstrap hold off timer expired
            lwm2m_state_set(LWM2M_STATE_BS_CONNECT);
            break;

        case LWM2M_STATE_BS_CONNECT_RETRY_WAIT:
            // Timeout waiting for DTLS connection to bootstrap server
            lwm2m_state_set(LWM2M_STATE_BS_CONNECT);
            break;

        case LWM2M_STATE_BOOTSTRAP_WAIT:
            // Timeout waiting for bootstrap ACK (CoAP)
            lwm2m_state_set(LWM2M_STATE_BS_CONNECTED);
            break;

        case LWM2M_STATE_BOOTSTRAPPING:
            // Timeout waiting for bootstrap to finish
            lwm2m_state_set(LWM2M_STATE_BOOTSTRAP_TIMEDOUT);
            break;

        case LWM2M_STATE_CLIENT_HOLD_OFF:
            // Client hold off timer expired
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT);
            break;

        case LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT:
            // Timeout waiting for DTLS connection to registration server
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT);
            break;

        case LWM2M_STATE_SERVER_REGISTER_WAIT:
            // Timeout waiting for registration ACK (CoAP)
            lwm2m_state_set(LWM2M_STATE_SERVER_CONNECTED);
            break;

        default:
            // Unknown timeout state
            break;
    }
}

#if APP_USE_SOCKET_POLL
static bool app_coap_socket_poll(void)
{
    struct nrf_pollfd fds[1+LWM2M_MAX_SERVERS];
    int nfds = 0;
    int ret = 0;

    // Find active sockets
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (m_lwm2m_transport[i] != -1) {
            fds[nfds].handle = m_lwm2m_transport[i];
            fds[nfds].requested = NRF_POLLIN;

            // Only check NRF_POLLOUT (writing possible) when waiting for connect()
            if ((i == m_security_instance) &&
                ((m_app_state == LWM2M_STATE_BS_CONNECT_WAIT) ||
                 (m_app_state == LWM2M_STATE_SERVER_CONNECT_WAIT))) {
                fds[nfds].events |= NRF_POLLOUT;
            }
            nfds++;
        }
    }

    if (nfds > 0) {
        ret = nrf_poll(fds, nfds, 1000);
    } else {
        // No active sockets to poll.
        lwm2m_os_sleep(1000);
    }

    if (ret == 0) {
        // Timeout; nothing more to check.
        return false;
    } else if (ret < 0) {
        LWM2M_ERR("poll error: %s (%d)",
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());
        return false;
    }

    bool data_ready = false;

    for (int i = 0; i < nfds; i++) {
        if ((fds[i].returned & NRF_POLLIN) == NRF_POLLIN) {
            // There is data to read.
            data_ready = true;
        }

        if ((fds[i].returned & NRF_POLLOUT) == NRF_POLLOUT) {
            // Writing is now possible.
            if (m_app_state == LWM2M_STATE_BS_CONNECT_WAIT) {
                LWM2M_INF("Connected");
                lwm2m_state_set(LWM2M_STATE_BS_CONNECTED);
            } else if (m_app_state == LWM2M_STATE_SERVER_CONNECT_WAIT) {
                LWM2M_INF("Connected");
                lwm2m_state_set(LWM2M_STATE_SERVER_CONNECTED);
            }
        }

        if ((fds[i].returned & NRF_POLLERR) == NRF_POLLERR) {
            // Error condition.
            lwm2m_state_t next_state;

            if (m_app_state == LWM2M_STATE_BS_CONNECT_WAIT) {
                next_state = LWM2M_STATE_BS_CONNECT_RETRY_WAIT;
            } else if (m_app_state == LWM2M_STATE_SERVER_CONNECT_WAIT) {
                next_state = LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT;
            } else {
                // TODO handle?
                LWM2M_ERR("NRF_POLLERR: %d", i);
                continue;
            }

            int error = 0;
            int len = sizeof(error);
            nrf_getsockopt(fds[i].fd, NRF_SOL_SOCKET, NRF_SO_ERROR, &error, &len);

            uint32_t coap_err_code = coap_security_destroy(fds[i].fd);
            ARG_UNUSED(coap_err_code);

            // NOTE: This works because we are only connecting to one server at a time.
            m_lwm2m_transport[m_security_instance] = -1;

            LWM2M_INF("Connection failed: %s (%d)", lwm2m_os_log_strdup(strerror(error)), errno);

            if (error == NRF_ENETDOWN) {
                return data_ready;
            }

            if (lwm2m_state_set(next_state)) {
                // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
                if (error == NRF_EINVAL || error == NRF_EOPNOTSUPP || error == NRF_ENETUNREACH) {
                    app_handle_connect_retry(m_security_instance, true);
                } else {
                    app_handle_connect_retry(m_security_instance, false);
                }

                if (error != NRF_ENETUNREACH) {
                    app_set_bootstrap_if_last_retry_delay(security_instance);
                }
            }
        }

        if ((fds[i].returned & NRF_POLLNVAL) == NRF_POLLNVAL) {
            // TODO: Ignore NRF_POLLNVAL for now.
            LWM2M_ERR("NRF_POLLNVAL: %d", i);
        }
    }

    return data_ready;
}
#endif

static void app_check_server_update(void)
{
    if ((m_app_state == LWM2M_STATE_REQUEST_DISCONNECT) ||
        (m_app_state == LWM2M_STATE_DISCONNECTED)) {
        // Disconnect requested or disconnected, nothing to check
        return;
    }

    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (m_connection_update[i].requested != LWM2M_REQUEST_NONE) {
            uint16_t server_instance = server_instance_get(i);
            if (m_lwm2m_transport[i] == -1) {
                if (m_app_state == LWM2M_STATE_IDLE) {
                    m_security_instance = i;
                    m_connection_update[i].requested = LWM2M_REQUEST_NONE;

                    int32_t client_hold_off_time = lwm2m_server_client_hold_off_timer_get(server_instance);
                    if (m_use_client_holdoff_timer && client_hold_off_time > 0) {
                        if (lwm2m_state_set(LWM2M_STATE_CLIENT_HOLD_OFF)) {
                            LWM2M_INF("Client hold off timer [%ld seconds] (server %u)", client_hold_off_time, i);
                            lwm2m_os_timer_start(state_update_timer, K_SECONDS(client_hold_off_time));
                        }
                    } else {
                        // No client hold off timer
                        lwm2m_state_set(LWM2M_STATE_SERVER_CONNECT);
                    }
                }
            } else if (lwm2m_server_registered_get(server_instance)) {
                if (m_connection_update[i].requested == LWM2M_REQUEST_DEREGISTER) {
                    m_security_instance = i;
                    m_connection_update[i].requested = LWM2M_REQUEST_NONE;
                    lwm2m_state_set(LWM2M_STATE_SERVER_DEREGISTER);
                    break; // Break the loop to process the deregister
                } else {
                    LWM2M_INF("Server update (server %u)", i);
                    m_connection_update[i].requested = LWM2M_REQUEST_NONE;
                    app_server_update(i, false);
                }
            }
        }
    }
}

static int app_lwm2m_process(void)
{
#if APP_USE_SOCKET_POLL
    if (app_coap_socket_poll()) {
        coap_input();
    }
#else
    coap_input();
#endif

    switch (m_app_state)
    {
        case LWM2M_STATE_REQUEST_LINK_UP:
        {
            (void)app_init_and_connect();
            break;
        }
        case LWM2M_STATE_REQUEST_LINK_DOWN:
        {
            (void)app_offline();
            break;
        }
        case LWM2M_STATE_REQUEST_CONNECT:
        {
            app_connect();
            break;
        }
        case LWM2M_STATE_BS_CONNECT:
        {
            LWM2M_INF("Bootstrap connect");
            app_bootstrap_connect();
            break;
        }
        case LWM2M_STATE_BOOTSTRAP_TIMEDOUT:
        {
            LWM2M_INF("Bootstrap timed out");
            app_disconnect();
            if (lwm2m_state_set(LWM2M_STATE_BS_CONNECT_RETRY_WAIT)) {
                app_handle_connect_retry(LWM2M_BOOTSTRAP_INSTANCE_ID, false);
            }
            break;
        }
        case LWM2M_STATE_BS_CONNECTED:
        {
            LWM2M_INF("Bootstrap register");
            app_bootstrap();
            break;
        }
        case LWM2M_STATE_SERVER_CONNECT:
        {
            LWM2M_INF("Server connect (server %u)", m_security_instance);
            app_server_connect(m_security_instance);
            break;
        }
        case LWM2M_STATE_SERVER_CONNECTED:
        {
            bool do_register = true;
            uint16_t server_instance = server_instance_get(m_security_instance);

            /* If already registered and having a remote location then do update. */
            if (lwm2m_server_registered_get(server_instance)) {

                uint16_t short_server_id = lwm2m_security_short_server_id_get(m_security_instance);

                // Register the remote.
                lwm2m_remote_register(short_server_id, (struct nrf_sockaddr *)&m_remote_server[m_security_instance]);

                // Load flash again, to retrieve the location.
                lwm2m_instance_storage_server_load(server_instance);

                char   * p_location;
                uint16_t location_len = 0;
                uint32_t err_code = lwm2m_remote_location_find(&p_location,
                                                               &location_len,
                                                               short_server_id);
                if (err_code == 0 && location_len > 0) {
                    do_register = false;
                }
            }

            if (do_register) {
                LWM2M_INF("Server register (server %u)", m_security_instance);
                app_server_register(m_security_instance);
            } else {
                LWM2M_INF("Server update after connect (server %u)", m_security_instance);
                app_server_update(m_security_instance, true);
            }

            break;
        }
        case LWM2M_STATE_SERVER_DEREGISTER:
        {
            LWM2M_INF("Server deregister (server %u)", m_security_instance);
            app_server_deregister(m_security_instance);
            break;
        }
        case LWM2M_STATE_REQUEST_DISCONNECT:
        {
            LWM2M_INF("Disconnect");
            app_disconnect();
            lwm2m_admin_pdn_deactivate();
            lwm2m_sms_receiver_disable();
            break;
        }
        case LWM2M_STATE_RESET:
        {
            lwm2m_system_reset(false);

            // Application has deferred the reset -> exit processing loop
            return -1;
        }
        default:
        {
            break;
        }
    }

    app_check_server_update();

    return 0;
}

static uint32_t app_coap_init(void)
{
    uint32_t err_code;

    err_code = coap_init(lwm2m_os_rand_get(), NULL,
                         lwm2m_os_malloc, lwm2m_os_free);

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        m_lwm2m_transport[i] = -1;
    }

    return err_code;
}

static int app_provision_psk(int sec_tag, char * identity, uint8_t identity_len, char * psk, uint8_t psk_len)
{
    int err_code;

    err_code = lwm2m_os_sec_identity_write(sec_tag, identity, identity_len);

    if (err_code != 0) {
        LWM2M_ERR("Unable to write Identity %d (%d)", sec_tag, err_code);
        return err_code;
    }

    size_t secret_key_nrf9160_style_len = psk_len * 2;
    uint8_t * p_secret_key_nrf9160_style = lwm2m_os_malloc(secret_key_nrf9160_style_len);
    for (int i = 0; i < psk_len; i++)
    {
        sprintf(&p_secret_key_nrf9160_style[i * 2], "%02x", psk[i]);
    }
    err_code = lwm2m_os_sec_psk_write(sec_tag, p_secret_key_nrf9160_style,
                                      secret_key_nrf9160_style_len);
    lwm2m_os_free(p_secret_key_nrf9160_style);

    if (err_code != 0) {
        LWM2M_ERR("Unable to write PSK %d (%d)", sec_tag, err_code);
        return err_code;
    }

    return 0;
}

static int app_provision_secret_keys(void)
{
    int ret = 0;
    int err = app_offline();

    if (err != 0) {
        return err;
    }

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        uint8_t identity_len = 0;
        uint8_t psk_len      = 0;
        char * p_identity    = lwm2m_security_identity_get(i, &identity_len);
        char * p_psk         = lwm2m_security_psk_get(i, &psk_len);

        if ((identity_len > 0) && (psk_len >0))
        {
            uint8_t uri_len; // Will be filled by server_uri query.
            char * p_server_uri = lwm2m_security_server_uri_get(i, &uri_len);
            char * p_server_uri_val = lwm2m_os_malloc(uri_len + 1);

            strncpy(p_server_uri_val, p_server_uri, uri_len);
            p_server_uri_val[uri_len] = 0;

            bool secure = false;
            uint16_t port = 0;
            const char * hostname = app_uri_get(p_server_uri_val, &port, &secure);
            ARG_UNUSED(hostname);

            if (secure) {
                err = app_provision_psk(APP_SEC_TAG_OFFSET + i, p_identity, identity_len, p_psk, psk_len);
                if (err == 0) {
                    LWM2M_TRC("Provisioning key for %s, short server id: %u",
                              lwm2m_os_log_strdup(p_server_uri_val),
                              lwm2m_security_short_server_id_get(i));
                } else {
                    ret = err;
                    LWM2M_ERR("Provisioning key failed (%d) for %s, short server id: %u (%d)", ret,
                              lwm2m_os_log_strdup(p_server_uri_val),
                              lwm2m_security_short_server_id_get(i));
                }
            }

            lwm2m_os_free(p_server_uri_val);
        }
    }
    LWM2M_INF("Wrote secret keys");

    if (ret != 0) {
        app_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, ret);
    }

    err = app_init_and_connect();
    if (ret == 0 && err != 0) {
        // Only set ret from app_init_and_connect() if not already set from a failing app_provision_psk().
        ret = err;
    }

    // THIS IS A HACK. Temporary solution to give a delay to recover Non-DTLS sockets from CFUN=4.
    // The delay will make TX available after CID again set.
    lwm2m_os_sleep(2000);

    return ret;
}

/**@brief Initializes app timers. */
static void app_timers_init(void)
{
    state_update_timer = lwm2m_os_timer_get(app_wait_state_update);
}

uint32_t lwm2m_net_reg_stat_get(void)
{
    return m_net_stat;
}

void lwm2m_net_reg_stat_cb(uint32_t net_stat)
{
    if (m_net_stat != net_stat)
    {
        if ((net_stat == APP_NET_REG_STAT_HOME) ||
            (lwm2m_debug_is_set(LWM2M_DEBUG_ROAM_AS_HOME) && (net_stat == APP_NET_REG_STAT_ROAM)))
        {
            // Home
            lwm2m_request_connect();
        }
        else if (net_stat == APP_NET_REG_STAT_ROAM)
        {
            // Roaming
            LWM2M_INF("Registered to roaming network");
            lwm2m_request_disconnect();
        }
        else if (net_stat != APP_NET_REG_STAT_SEARCHING)
        {
            // Not registered
            LWM2M_INF("No network (%d)", net_stat);
            lwm2m_request_disconnect();
        }
        else
        {
            LWM2M_INF("Searching for network...");
        }

        m_net_stat = net_stat;
    }
    else
    {
        LWM2M_TRC("Network registration status (%d)", net_stat);
    }
}

void lwm2m_non_rst_message_cb(void *data)
{
    lwm2m_observer_storage_delete((coap_observer_t*)data);
}

int lwm2m_carrier_init(const lwm2m_carrier_config_t * config)
{
    int err;
    enum lwm2m_firmware_update_state mdfu;

    if ((config != NULL) && (config->bootstrap_uri != NULL)) {
        m_app_config.bootstrap_uri = config->bootstrap_uri;
    }

    if ((config != NULL) && (config->psk != NULL)) {
        m_app_config.psk        = config->psk;
        m_app_config.psk_length = config->psk_length;
    }

    // Initialize OS abstraction layer.
    // This will initialize the NVS subsystem as well.
    lwm2m_os_init();

    app_timers_init();

    err = lwm2m_firmware_update_state_get(&mdfu);
    if (!err && mdfu == UPDATE_SCHEDULED) {
        LWM2M_INF("Update scheduled, please wait..\n");
        lwm2m_state_set(LWM2M_STATE_MODEM_FIRMWARE_UPDATE);
    }

    err = lwm2m_os_bsdlib_init();
    if (err < 0) {
        /* bsdlib failed to initialize, fatal error */
        return -1;
    }

    if (err > 0) {
        /* We have completed a modem firmware update.
         * Whatever the result, update the state and reboot.
         */
        lwm2m_firmware_update_state_set(UPDATE_EXECUTED);
        lwm2m_os_sys_reset();
        CODE_UNREACHABLE;
    }

    (void)app_event_notify(LWM2M_CARRIER_EVENT_BSDLIB_INIT, NULL);

    // Initialize AT interface
    err = at_if_init();
    if (err) {
        return err;
    }

    // Register network registration status changes
    at_subscribe_net_reg_stat(lwm2m_net_reg_stat_cb);

    // Initialize debug settings from flash.
    app_debug_init();

    // Provision certificates for DFU before turning the Modem on.
    cert_provision();

    // Set-phone-functionality. Blocking call until we are connected.
    // The lc module uses AT notifications.
    lwm2m_state_set(LWM2M_STATE_DISCONNECTED);

    err = app_init_and_connect();
    if (err != 0) {
        return err;
    }

    // Read IMEI, which is a static value and will never change.
    err = at_read_imei(m_imei, sizeof(m_imei));
    if (err != 0) {
        // IMEI is required to generate a unique Client ID. Cannot continue.
        LWM2M_ERR("Unable to read IMEI, cannot generate client ID");
        return -EIO;
    }

    // Initialize CoAP.
    err = app_coap_init();
    if (err) {
        return err;
    }

    // Setup LWM2M endpoints.
    app_lwm2m_setup();

    // Setup LWM2M storage subsystem.
    lwm2m_instance_storage_init();

    // Register handler for RST messages on NON confirmable CoAP messages
    coap_reset_message_handler_register(lwm2m_non_rst_message_cb);

    // Create LwM2M factory bootstrapped objects.
    app_lwm2m_create_objects();

    if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_IPv6)) {
        for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
            m_family_type[i] = NRF_AF_INET;
        }
    }

    return 0;
}

void lwm2m_carrier_run(void)
{
    for (;;)
    {
#if (APP_USE_SOCKET_POLL == 0)
        // If poll is disabled, sleep.
        lwm2m_os_sleep(10);
#endif

        if (tick_count++ % 100 == 0) {
            // Pass a tick to CoAP in order to re-transmit any pending messages.
            ARG_UNUSED(coap_time_tick());
        }

        int exit = app_lwm2m_process();

        if (exit != 0)
        {
            break;
        }

        if ((tick_count % 100) == 0)
        {
            lwm2m_observer_process(false);
        }
    }
}
