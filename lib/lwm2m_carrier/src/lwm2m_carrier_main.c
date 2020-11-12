/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_tlv.h>
#include <lwm2m_access_control.h>
#include <lwm2m_api.h>
#include <lwm2m_observer.h>
#include <lwm2m_observer_storage.h>
#include <lwm2m_carrier.h>
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
#include <sys/byteorder.h>

#include <lwm2m_carrier_client.h>
#include <lwm2m_client_util.h>

#include <app_debug.h>
#include <at_interface.h>
#include <operator_check.h>
#include <nrf_socket.h>
#include <nrf_errno.h>
#include <sms_receive.h>
#include <coap_observe_api.h>

#include <sha256.h>

#define APP_SEC_TAG_OFFSET              25
#define APP_BOOTSTRAP_SEC_TAG           (APP_SEC_TAG_OFFSET + 0)                              /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_DIAGNOSTICS_SEC_TAG         (APP_SEC_TAG_OFFSET + 2)                              /**< Tag used to identify security credentials used by the client for diagnostics server. */

#define APP_NET_REG_STAT_HOME           1                                                   /**< Registered to home network. */
#define APP_NET_REG_STAT_SEARCHING      2                                                   /**< Searching an operator to register. */
#define APP_NET_REG_STAT_ROAM           5                                                   /**< Registered to roaming network. */

#define APP_CLIENT_ID_LENGTH            128                                                 /**< Buffer size to store the Client ID. */

/* Initialize config with default values. */
static lwm2m_carrier_config_t              m_app_config;
static bool                                m_application_psk_set;

lwm2m_client_identity_t                    m_client_id;                                       /**< Client ID structure to hold the client's UUID. */

// Objects
static lwm2m_object_t                      m_bootstrap_server;                                /**< Named object to be used as callback object when bootstrap is completed. */

static char m_bootstrap_object_alias_name[] = "bs";                                           /**< Name of the bootstrap complete object. */

static volatile lwm2m_state_t m_app_state = LWM2M_STATE_BOOTING;                              /**< Application state. Should be one of @ref lwm2m_state_t. */
static volatile bool          m_did_bootstrap;

/** @brief 15 digits IMEI stored as a NULL-terminated String. */
static char m_imei[16];

/**
 * @brief Subscriber number (MSISDN) stored as a NULL-terminated String.
 * Number with max 15 digits. Length varies depending on the operator and country.
 * VZW allows only 10 digits.
 */
static char m_msisdn[16];

static uint32_t m_net_stat;

static bool m_lte_ready;
static bool m_ack_sms;

static volatile uint32_t tick_count;

typedef enum {
    PSK_FORMAT_BINARY,
    PSK_FORMAT_HEXSTRING
} psk_format_t;

static int app_provision_psk(int sec_tag, const char * p_identity, uint8_t identity_len,
                             const char * p_psk, uint8_t psk_len, psk_format_t psk_format);
static int app_provision_secret_keys(void);

static void app_carrier_objects_setup(void);

extern int cert_provision();

int lwm2m_main_event_notify(uint32_t type, void * data)
{
    lwm2m_carrier_event_t event =
    {
        .type = type,
        .data = data
    };

    return lwm2m_carrier_event_handler(&event);
}

int lwm2m_main_event_error(uint32_t error_code, int32_t error_value)
{
    // Put library in ERROR state to exit lwm2m_carrier_run loop
    m_app_state = LWM2M_STATE_ERROR;

    lwm2m_carrier_event_error_t error_event = {
        .code = error_code,
        .value = error_value
    };

    return lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_ERROR, &error_event);
}

void lwm2m_acknowledge_sms(void)
{
    m_ack_sms = true;
}

static bool lwm2m_state_set(lwm2m_state_t app_state)
{
    // Do not allow state change if network state has changed, or on error.
    // This may have happened during a blocking socket operation, typically
    // connect(), and then we must abort any ongoing state changes.
    if ((m_app_state == LWM2M_STATE_REQUEST_CONNECT) ||
        (m_app_state == LWM2M_STATE_REQUEST_DISCONNECT) ||
        (m_app_state == LWM2M_STATE_ERROR))
    {
        return false;
    }

    m_app_state = app_state;

    return true;
}

static int app_init_and_connect(void)
{
    lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_CONNECTING, NULL);

    int err = lwm2m_os_lte_link_up();

    lwm2m_apn_conn_prof_activate(lwm2m_apn_conn_prof_default_instance(), 0);

    if (err == 0) {
        lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_CONNECTED, NULL);
    } else {
        lwm2m_main_event_error(LWM2M_CARRIER_ERROR_CONNECT_FAIL, err);
    }

    return err;
}

static int app_offline(void)
{
    lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_DISCONNECTING, NULL);

    // Set state to DISCONNECTED to avoid detecting "no registered network"
    // when provisioning security keys.
    lwm2m_state_set(LWM2M_STATE_DISCONNECTED);

    lwm2m_apn_conn_prof_deactivate(lwm2m_apn_conn_prof_default_instance());

    int err = lwm2m_os_lte_link_down();

    if (err == 0) {
        lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_DISCONNECTED, NULL);
    } else {
        lwm2m_main_event_error(LWM2M_CARRIER_ERROR_DISCONNECT_FAIL, err);
    }

    return err;
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
        lwm2m_bootstrap_clear();
        lwm2m_remote_location_clear();
        lwm2m_storage_location_delete();

        lwm2m_client_configure();
        lwm2m_request_connect();
    }
}

void lwm2m_request_connect(void)
{
    // Todo: Handle request connect when doing connect()
    m_app_state = LWM2M_STATE_REQUEST_CONNECT;
}

void lwm2m_request_disconnect(void)
{
    // Only request disconnect if not already disconnected and not error
    if ((m_app_state != LWM2M_STATE_DISCONNECTED) &&
        (m_app_state != LWM2M_STATE_ERROR))
    {
        m_app_state = LWM2M_STATE_REQUEST_DISCONNECT;
    }
}

void lwm2m_request_reset(void)
{
    if (operator_is_vzw(true)) {
        // Trigger full Register at next boot instead of doing Update
        for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++) {
            lwm2m_server_registered_set(i, false);
            lwm2m_remote_deregister(lwm2m_server_short_server_id_get(i));
        }
        lwm2m_storage_server_store();
        lwm2m_storage_location_delete();
    }

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

void lwm2m_system_shutdown(void)
{
    // Do not close sockets on shutdown to keep the DTLS session cache

    lwm2m_os_lte_power_down();
    lwm2m_os_bsdlib_shutdown();

    m_app_state = LWM2M_STATE_SHUTDOWN;

    LWM2M_INF("LTE link down");
}

void lwm2m_system_reset(bool force_reset)
{
    int ret = lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_REBOOT, NULL);

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

#if 0 // Todo: Implement use of this
static void vzw_initialize_class3(void)
{
    char modem_class3_apn[64];
    char stored_class3_apn[64];
    int modem_class3_len;
    int stored_class3_len;

    stored_class3_len = lwm2m_stored_class3_apn_read(stored_class3_apn,
                                                     sizeof(stored_class3_apn));

    if (stored_class3_len > 0)
    {
        modem_class3_len = sizeof(modem_class3_apn);
        int result = at_read_apn_class(3, modem_class3_apn, &modem_class3_len);

        if ((result == 0) && (modem_class3_len > 0) &&
            ((stored_class3_len != modem_class3_len) ||
             (strncmp(modem_class3_apn, stored_class3_apn, stored_class3_len) != 0)))
        {
            // CLASS3 APN is different in Modem and local storage
            LWM2M_INF("Updated APN table");
            lwm2m_conn_mon_class_apn_set(3, stored_class3_apn, stored_class3_len);
        }
    }
}
#endif

/* Read the access point name into a buffer, and null-terminate it.
 * Returns the length of the access point name.
 */
int lwm2m_carrier_apn_get(char *buf, size_t len)
{
    char *p_apn;
    uint8_t apn_len = 0;

    if (operator_is_vzw(false)) {
        p_apn = lwm2m_conn_mon_class_apn_get(2, &apn_len);
    } else {
        p_apn = lwm2m_apn_conn_prof_apn_get(lwm2m_apn_instance(), &apn_len);
    }

    if (len < apn_len + 1) {
        return -1;
    }

    memcpy(buf, p_apn, apn_len);
    buf[apn_len] = '\0';

    return apn_len;
}

bool lwm2m_request_remote_reconnect(struct nrf_sockaddr *p_remote)
{
    bool requested = false;

    uint16_t short_server_id = 0;
    uint16_t security_instance = lwm2m_security_instance_from_remote(p_remote, &short_server_id);

    if (lwm2m_client_reconnect(security_instance) == 0) {
        requested = true;
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
        lwm2m_main_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, ret);
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
            app_carrier_objects_setup();
        }
        if (lwm2m_factory_bootstrap_update(&m_app_config, m_application_psk_set)) {
            lwm2m_last_used_msisdn_set("", 0);
            clear_bootstrap = true;
        }

        lwm2m_last_used_operator_id_set(operator_id(true));
        lwm2m_remote_location_clear();
        lwm2m_storage_location_delete();
    }

    if (!app_bootstrap_keys_exists()) {
        provision_bs_psk = true;
    }

    if (operator_is_vzw(true)) {
        // Get the MSISDN with correct format for VZW.
        char * p_msisdn = lwm2m_msisdn_get();

        len = lwm2m_last_used_msisdn_get(last_used_msisdn, sizeof(last_used_msisdn));
        len = MIN(len, 15);
        last_used_msisdn[len] = '\0';
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
        lwm2m_client_configure();
        lwm2m_retry_delay_connect_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);
        provision_bs_psk = true;
    }

    if (provision_bs_psk) {
        int err = app_offline();
        if (err != 0) {
            return err;
        }

        uint8_t psk_len = (m_app_config.psk ? strlen(m_app_config.psk) : 0);
        ret = app_provision_psk(APP_BOOTSTRAP_SEC_TAG, client_id, strlen(client_id),
                                m_app_config.psk, psk_len, PSK_FORMAT_HEXSTRING);

        if ((ret == 0) && operator_is_att(true) && (m_app_config.psk == NULL)) {
            // Generate AT&T Bootstrap PSK in Modem
            LWM2M_INF("Generating bootstrap PSK");
            ret = at_bootstrap_psk_generate(APP_BOOTSTRAP_SEC_TAG);
        }

        if ((ret == 0) && operator_is_vzw(true)) {
            char app_diagnostics_psk[SHA256_BLOCK_SIZE];
            app_vzw_sha256_psk(m_imei, 101, app_diagnostics_psk);
            ret = app_provision_psk(APP_DIAGNOSTICS_SEC_TAG, m_imei, strlen(m_imei),
                                    app_diagnostics_psk, sizeof(app_diagnostics_psk), PSK_FORMAT_BINARY);
        }

        if (ret != 0) {
            lwm2m_main_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, ret);
        }

        err = app_init_and_connect();
        if (ret == 0 && err != 0) {
            // Only set ret from app_init_and_connect() if not already set from a failing app_provision_psk().
            ret = err;
        }
    }

    return ret;
}

/**@brief Delete all Security, Server and Access Control instances.
 *
 * @param[in]  delete_bootstrap  Set if deleting bootstrap instances.
 */
static void delete_bootstrapped_object_instances(void)
{
    lwm2m_instance_t *p_instance;
    uint16_t bootstrap_ssid = lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID);

    // Delete all instances except Bootstrap server
    for (uint32_t i = 0; i < 1 + LWM2M_MAX_SERVERS; i++)
    {
        if (i != LWM2M_BOOTSTRAP_INSTANCE_ID)
        {
            p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(i);
            (void)lwm2m_coap_handler_instance_delete(p_instance);
        }

        if (lwm2m_server_short_server_id_get(i) != bootstrap_ssid)
        {
            p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(i);
            (void)lwm2m_coap_handler_instance_delete(p_instance);
        }
    }

    // Delete all Access Control instances
    lwm2m_access_control_delete_instances();
}

/**@brief Application implementation of the root handler interface.
 *
 * @details This function is not bound to any object or instance. It will be called from
 *          LWM2M upon an action on the root "/" URI path. During bootstrap it is expected
 *          to get a DELETE operation on this URI.
 */
uint32_t lwm2m_coap_handler_root(uint8_t op_code, coap_message_t * p_request)
{
    delete_bootstrapped_object_instances();

    (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);

    return 0;
}

const void * observable_reference_get(const uint16_t *p_path, uint8_t path_len, uint8_t *p_type)
{
    const void * value = NULL;
    lwm2m_object_t *object;
    lwm2m_instance_t *instance;
    int ret;

    if (!p_path || path_len <= 0)
    {
        return NULL;
    }

    if (p_type)
    {
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
    }

    if (path_len == 1)
    {
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
    case LWM2M_OBJ_PORTFOLIO:
        value = lwm2m_portfolio_resource_reference_get(p_path[1], p_path[2], p_type);
        break;
    case LWM2M_OBJ_SECURITY:
    case LWM2M_OBJ_ACCESS_CONTROL:
    case LWM2M_OBJ_LOCATION:
    case LWM2M_OBJ_CONN_STAT:
    default:
        // Unsupported observables.
        break;
    }

    return value;
}

static void lwm2m_notif_attribute_default_value_set(uint8_t type, void * p_value, struct nrf_sockaddr *p_remote_server)
{
    // TODO: Assert value != NULL && p_remote_server != NULL
    uint16_t security_instance, server_instance, short_server_id;

    security_instance = lwm2m_security_instance_from_remote(p_remote_server, &short_server_id);
    // Todo: Get server instance
    server_instance = 1; // server_instance_get(security_instance);

    switch (type)
    {
    case LWM2M_ATTR_TYPE_MIN_PERIOD:
        *(int32_t *)p_value = lwm2m_server_get_instance(server_instance)->default_minimum_period;
        break;
    case LWM2M_ATTR_TYPE_MAX_PERIOD:
        *(int32_t *)p_value = lwm2m_server_get_instance(server_instance)->default_maximum_period;
        break;
    case LWM2M_ATTR_TYPE_GREATER_THAN:
        *(int32_t *)p_value = INT32_MAX;
        break;
    case LWM2M_ATTR_TYPE_LESS_THAN:
        *(int32_t *)p_value = -INT32_MAX;
        break;
    case LWM2M_ATTR_TYPE_STEP:
        *(int32_t *)p_value = INT32_MAX;
        break;
    default:
        // TODO: Assert for unsupported type.
        break;
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

void lwm2m_set_bootstrapped(bool bootstrapped)
{
    lwm2m_storage_misc_data_t misc_data = { 0 };

    lwm2m_storage_misc_data_load(&misc_data);
    if (misc_data.bootstrapped != (uint8_t) bootstrapped) {
        misc_data.bootstrapped = bootstrapped;
        lwm2m_storage_misc_data_store(&misc_data);
    }
}

uint32_t bootstrap_object_callback(lwm2m_object_t * p_object,
                                   uint16_t         instance_id,
                                   uint8_t          op_code,
                                   coap_message_t * p_request)
{
    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

    lwm2m_client_bootstrap_done();

    return 0;
}

void lwm2m_bootstrap_clear(void)
{
    lwm2m_set_bootstrapped(false);
    lwm2m_security_bootstrapped_set(false);
}

void lwm2m_main_bootstrap_reset(void)
{
    if (lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID) == 0) {
        // Server object not loaded yet
        lwm2m_storage_security_load();
    }

    if (lwm2m_security_bootstrapped_get()) {
        // Security object exists and bootstrap is done
        lwm2m_security_bootstrapped_set(false);
    }

    lwm2m_set_bootstrapped(false);
    delete_bootstrapped_object_instances();
    lwm2m_factory_bootstrap_init(&m_app_config);

    lwm2m_device_update_carrier_specific_settings();

    for (uint32_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        lwm2m_observer_delete(i);
    }
}

void lwm2m_main_bootstrap_done(void)
{
    m_did_bootstrap = true;

    LWM2M_INF("Store bootstrap settings");

    lwm2m_storage_server_store();
    lwm2m_storage_security_store();

    lwm2m_access_control_acl_init();
    lwm2m_storage_access_control_store();

    if (app_provision_secret_keys() == 0) {
        (void)lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_BOOTSTRAPPED, NULL);
    }

    // For VzW in certification mode we don't set bootstrapped here because
    // the Motive framework may have issues creating the device correctly,
    // and we want the device to start doing bootstrap until successfully
    // registered to all servers.

    // For VzW live and other carriers we set bootstrapped here because
    // the bootstrapped objects has been stored correctly both on the device
    // and on the server. This will avoid unnecessary bootstrap.

    if (!operator_is_vzw(true) ||
        !(m_app_config.certification_mode || lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)))
    {
        // Bootstrap is done.
        lwm2m_set_bootstrapped(true);
    }

}

static void app_access_control_context_set(void)
{
    if (operator_is_lgu(true))
    {
        lwm2m_ctx_access_control_enable_status_set(false);
    }
    else
    {
        lwm2m_ctx_access_control_enable_status_set(true);
    }
}

static void app_carrier_objects_setup(void)
{
    lwm2m_object_t *p_object;

    if (operator_is_att(true))
    {
        int32_t ext_dev_info;

        // Set ExtDevInfo
        ext_dev_info = LWM2M_OBJ_PORTFOLIO << 16;
        lwm2m_device_ext_dev_info_set(&ext_dev_info, 1);

        // Add APN connection profile support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_APN_CONNECTION_PROFILE) != 0) {
            lwm2m_apn_conn_prof_init();
            (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_apn_conn_prof_get_object());
            lwm2m_storage_apn_conn_prof_load();
        }

        // Add portfolio support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_PORTFOLIO) != 0) {
            lwm2m_portfolio_init();
            (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_portfolio_get_object());
            lwm2m_storage_portfolio_load();
        }

        // Add connectivity extension support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_CONN_EXT) != 0) {
            lwm2m_conn_ext_init();
            (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_ext_get_object());
            lwm2m_storage_conn_ext_load();
        }
    }
    else
    {
        size_t progress;
        lwm2m_instance_t *p_instance = NULL;

        lwm2m_device_ext_dev_info_clear();

        /* Remove all object instances from the request handler. */
        while (lwm2m_instance_next(&p_instance, &progress))
        {
            if ((p_instance->object_id == LWM2M_OBJ_APN_CONNECTION_PROFILE) ||
                (p_instance->object_id == LWM2M_OBJ_PORTFOLIO) ||
                (p_instance->object_id == LWM2M_OBJ_CONN_EXT))
            {
                lwm2m_coap_handler_instance_delete(p_instance);

                // Decrease progress by one because delete will move current
                // last entry into this index position.
                progress--;
            }
        }

        // Remove APN connection profile support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_APN_CONNECTION_PROFILE) == 0) {
            (void)lwm2m_coap_handler_object_delete(p_object);
            (void)lwm2m_storage_portfolio_delete();
        }

        // Remove portfolio support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_PORTFOLIO) == 0) {
            (void)lwm2m_coap_handler_object_delete(p_object);
            (void)lwm2m_storage_apn_conn_prof_delete();
        }

        // Remove connectivity extension support.
        if (lwm2m_lookup_object(&p_object, LWM2M_OBJ_CONN_EXT) == 0) {
            (void)lwm2m_coap_handler_object_delete(p_object);
            (void)lwm2m_storage_conn_ext_delete();
        }
    }

    // Set corresponding access control context according to operator.
    app_access_control_context_set();
}

void lwm2m_factory_reset(void)
{
    lwm2m_set_bootstrapped(false);
    lwm2m_stored_class3_apn_delete();

    // Provision bootstrap PSK and diagnostic PSK at next startup
    lwm2m_last_used_msisdn_set("", 0);
    lwm2m_last_used_operator_id_set(OPERATOR_ID_UNSET);

    // Delete data from flash
    lwm2m_storage_security_delete();
    lwm2m_storage_server_delete();
    lwm2m_storage_access_control_delete();
    lwm2m_storage_apn_conn_prof_delete();
    lwm2m_storage_portfolio_delete();
    lwm2m_storage_conn_ext_delete();
    lwm2m_observer_storage_delete_all();
    lwm2m_notif_attr_storage_delete_all();
    lwm2m_storage_location_delete();
}

static void app_load_flash_objects(void)
{
    LWM2M_INF("Load bootstrap settings");

    lwm2m_storage_security_load();
    lwm2m_storage_server_load();

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        if (lwm2m_security_short_server_id_get(i) != 0)
        {
            lwm2m_coap_handler_instance_add(&lwm2m_security_get_instance(i)->proto);
        }

        if (lwm2m_server_short_server_id_get(i) != 0)
        {
            lwm2m_coap_handler_instance_add(&lwm2m_server_get_instance(i)->proto);
        }
    }

    // Load location
    lwm2m_storage_location_load();

    lwm2m_storage_misc_data_t misc_data;
    int32_t result = lwm2m_storage_misc_data_load(&misc_data);
    if (result == 0 && misc_data.bootstrapped)
    {
        lwm2m_security_bootstrapped_set(true);
        lwm2m_storage_access_control_load();
    }
    else
    {
        // storage reports that bootstrap has not been done, continue with bootstrap.
        lwm2m_security_bootstrapped_set(false);
    }
}

static void app_lwm2m_create_objects(void)
{
    // Init functions will check operator ID.
    operator_id_read();

    lwm2m_security_init();
    lwm2m_server_init();
    lwm2m_access_control_init();
    lwm2m_device_init();
    lwm2m_conn_mon_init();
    lwm2m_firmware_init();
    lwm2m_conn_stat_init();

    // Initialize objects from flash, if they exist in NVS.
    app_load_flash_objects();

    // Setup operator-specific objects.
    app_carrier_objects_setup();
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

    m_bootstrap_server.object_id    = LWM2M_NAMED_OBJECT;
    m_bootstrap_server.callback     = bootstrap_object_callback;
    m_bootstrap_server.p_alias_name = m_bootstrap_object_alias_name;
    (void)lwm2m_coap_handler_object_add(&m_bootstrap_server);

    // Add security support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_security_get_object());

    // Add server support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_server_get_object());

    // Add access control support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_access_control_get_object());

    // Add device support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_device_get_object());

    // Add connectivity monitoring support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_mon_get_object());

    // Add firmware support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_firmware_get_object());

    // Add connectivity statistics support.
    (void)lwm2m_coap_handler_object_add((lwm2m_object_t *)lwm2m_conn_stat_get_object());

    // Add callback to set default notification attribute values.
    lwm2m_observer_notif_attr_default_cb_set(lwm2m_notif_attribute_default_value_set);

    // Add callback to get pointers to observables.
    lwm2m_observer_observable_get_cb_set(observable_reference_get);

    // Add callback to get the uptime in milliseconds and initialize the timer.
    lwm2m_observer_uptime_cb_init(lwm2m_os_uptime_get);

    // Add callback to request remote server reconnection.
    lwm2m_request_remote_reconnect_cb_set(lwm2m_request_remote_reconnect);
}

static void app_connect(void)
{
    // Read operator ID.
    operator_id_read();

    if (operator_is_supported(true) &&
        ((m_net_stat == APP_NET_REG_STAT_HOME) ||
         (lwm2m_debug_is_set(LWM2M_DEBUG_ROAM_AS_HOME) && (m_net_stat == APP_NET_REG_STAT_ROAM))))
    {
        LWM2M_INF("Registered to home network (%s)", lwm2m_os_log_strdup(operator_id_string(OPERATOR_ID_CURRENT)));
        if (operator_is_supported(false)) {
            lwm2m_sms_receiver_enable();
        }

        // Todo: Avoid calling lwm2m_client_connect() twice when write keys.
        app_generate_client_id();
        lwm2m_client_connect();
        m_app_state = LWM2M_STATE_IDLE;

        if (!m_lte_ready && lwm2m_security_bootstrapped_get()) {
            m_lte_ready = true;
            (void)lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_LTE_READY, NULL);
        }
    } else {
        LWM2M_INF("Waiting for home network");
        lwm2m_sms_receiver_disable();
        lwm2m_client_disconnect();
        m_app_state = LWM2M_STATE_DISCONNECTED;
    }
}

static void app_disconnect(void)
{
    LWM2M_INF("Disconnect");
    lwm2m_sms_receiver_disable();
    lwm2m_client_disconnect();

    m_app_state = LWM2M_STATE_DISCONNECTED;
}

static int app_lwm2m_process(void)
{
    coap_input();

    if (m_ack_sms) {
        m_ack_sms = false;
        int err = lwm2m_os_at_cmd_write("AT+CNMA=1", NULL, 0);
        if (err) {
            LWM2M_WRN("Acking SMS failed with err %d", err);
        }
    }

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
        case LWM2M_STATE_REQUEST_DISCONNECT:
        {
            app_disconnect();
            break;
        }
        case LWM2M_STATE_RESET:
        {
            lwm2m_system_reset(false);

            // Application has deferred the reset -> exit processing loop
            return -1;
        }
        case LWM2M_STATE_ERROR:
        {
            lwm2m_sms_receiver_disable();
            lwm2m_client_disconnect();

            // Unrecoverable error, exit processing loop
            m_app_state = LWM2M_STATE_ERROR;
            return -1;
        }
        default:
        {
            break;
        }
    }

    lwm2m_pdn_check_closed();

    return 0;
}

static uint32_t app_coap_init(void)
{
    uint32_t err_code;
    uint16_t seed;
    uint32_t stamp;

    stamp = lwm2m_os_rand_get();
    /* Use the digits that vary the most (last two bytes) and swap them
     * to decrease the chance they will be the same across reboots.
     * Do not call lwm2m_os_rand_get() inside sys_cpu_to_be16(),
     * or it will be called twice.
     */
    seed = sys_cpu_to_be16(stamp);

    err_code = coap_init(seed, NULL, lwm2m_os_malloc, lwm2m_os_free);

    return err_code;
}

static int app_provision_psk(int sec_tag, const char * p_identity, uint8_t identity_len,
                             const char * p_psk, uint8_t psk_len, psk_format_t psk_format)
{
    int err_code;

    err_code = lwm2m_os_sec_identity_write(sec_tag, p_identity, identity_len);

    if (err_code != 0) {
        LWM2M_ERR("Unable to write Identity %d (%d)", sec_tag, err_code);
        return err_code;
    }

    if ((p_psk == NULL) || (psk_len == 0)) {
        // No PSK to write
        (void)lwm2m_os_sec_psk_delete(sec_tag);
        return 0;
    }

    switch (psk_format)
    {
        case PSK_FORMAT_BINARY:
        {
            // Convert PSK to null-terminated hex string
            size_t hex_string_len = psk_len * 2;
            uint8_t hex_string[hex_string_len];

            for (int i = 0; i < psk_len; i++) {
                sprintf(&hex_string[i * 2], "%02x", p_psk[i]);
            }

            err_code = lwm2m_os_sec_psk_write(sec_tag, hex_string, hex_string_len);
            break;
        }

        case PSK_FORMAT_HEXSTRING:
        {
            // PSK is already null-terminated hex string
            err_code = lwm2m_os_sec_psk_write(sec_tag, p_psk, psk_len);
            break;
        }
    }

    if (err_code != 0) {
        LWM2M_ERR("Unable to write PSK %d (%d)", sec_tag, err_code);
    }

    return err_code;
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
            const char * hostname = client_parse_uri(p_server_uri_val, uri_len, &port, &secure);
            ARG_UNUSED(hostname);

            if (secure) {
                err = app_provision_psk(APP_SEC_TAG_OFFSET + i, p_identity, identity_len,
                                        p_psk, psk_len, PSK_FORMAT_BINARY);
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
        lwm2m_main_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, ret);
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

uint32_t lwm2m_net_reg_stat_get(void)
{
    return m_net_stat;
}

void lwm2m_net_reg_stat_cb(uint32_t net_stat)
{
    if (m_app_state == LWM2M_STATE_ERROR)
    {
        // Nothing to do
        return;
    }

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

            if (!m_lte_ready) {
                m_lte_ready = true;
                (void)lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_LTE_READY, NULL);
            }
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


/** @brief Initializes m_app_config and copies a given config.
 *
 * Memory pointed to by pointers is not copied.
 *
 * @param[in] p_config Pointer to the config that should be copied.
 */
static void init_config_set(const lwm2m_carrier_config_t * const p_config)
{
    if (p_config == NULL){
        return;
    }

    if (p_config->bootstrap_uri != NULL) {
        m_app_config.bootstrap_uri = p_config->bootstrap_uri;
    }

    if (p_config->psk != NULL) {
        m_app_config.psk        = p_config->psk;
        m_application_psk_set   = true;
    }

    if (p_config->apn != NULL) {
        m_app_config.apn = p_config->apn;
    }

    m_app_config.certification_mode = p_config->certification_mode;
}

void app_carrier_apn_init(void)
{
    if (operator_is_att(true))
    {
        // Read APN status and reflect it in the APN Connection Profile object.
        lwm2m_apn_conn_prof_apn_status_update();

        // Register the custom APN if it has been provided.
        if (m_app_config.apn)
        {
            int err = lwm2m_apn_conn_prof_custom_apn_set(m_app_config.apn);
            if (err != 0) {
                LWM2M_ERR("Unable to setup custom APN, err %d", err);
            }
        }

        lwm2m_pdn_first_enabled_apn_instance();
    }
}

int lwm2m_carrier_init(const lwm2m_carrier_config_t * config)
{
    int err;
    enum lwm2m_firmware_update_state mdfu;

    // Check for configuration from the application.
    init_config_set(config);

    // Initialize OS abstraction layer.
    // This will initialize the NVS subsystem as well.
    lwm2m_os_init();

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

    (void)lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_BSDLIB_INIT, NULL);

    // Initialize DFU socket and firmware download.
    // This will erase old firmware as necessary, so do it before connecting.
    err = lwm2m_firmware_download_init();
    if (err) {
        return err;
    }

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

    lwm2m_pdn_init();

    // Initialize CoAP.
    err = app_coap_init();
    if (err) {
        return err;
    }

    // Certain objects are used in specific networks.
    operator_id_read();

    // Setup LWM2M endpoints.
    app_lwm2m_setup();

    // Setup LWM2M storage subsystem.
    lwm2m_instance_storage_init();

    // Register handler for RST messages on NON confirmable CoAP messages
    coap_reset_message_handler_register(lwm2m_non_rst_message_cb);

    // Create LwM2M factory bootstrapped objects.
    app_lwm2m_create_objects();

    // Initialise carrier-specific APN handling.
    app_carrier_apn_init();

    // Default APN is connected at startup.
    lwm2m_apn_conn_prof_activate(lwm2m_apn_conn_prof_default_instance(), 0);

    // Resume modem firmware updates if necessary
    // Do this when we have a connection.
    lwm2m_firmware_download_resume();

    lwm2m_client_init();
    lwm2m_client_configure();

    return 0;
}

void lwm2m_carrier_run(void)
{
    for (;;)
    {
#if (APP_USE_SOCKET_POLL == 0)
        // If poll is disabled, sleep.
        lwm2m_os_sleep(500);
#endif

        if (tick_count++ % 2 == 0) {
            // Pass a tick to CoAP in order to re-transmit any pending messages.
            ARG_UNUSED(coap_time_tick());
        }

        int exit = app_lwm2m_process();

        if (exit != 0)
        {
            break;
        }

        if ((tick_count % 2) == 0)
        {
            lwm2m_observer_process(false);
        }
    }

    LWM2M_ERR("Exit");
}
