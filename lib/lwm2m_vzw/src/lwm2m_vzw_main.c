/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <string.h>

#include <lwm2m_tlv.h>
#include <lwm2m_acl.h>
#include <lwm2m_api.h>
#include <lwm2m_conn_mon.h>
#include <lwm2m_vzw_main.h>
#include <lwm2m_device.h>
#include <lwm2m_firmware.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_remote.h>
#include <lwm2m_retry_delay.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>

#include <app_debug.h>
#include <at_interface.h>
#include <logging/log.h>
#include <lte_lc.h>
#include <nrf_inbuilt_key.h>
#include <pdn_management.h>

LOG_MODULE_REGISTER(lwm2m_vzw_main);

#define APP_USE_SOCKET_POLL             0 // Use socket poll() to check status
#define APP_ACL_DM_SERVER_HACK          1
#define APP_USE_CONTABO                 0

#define COAP_LOCAL_LISTENER_PORT              5683                                            /**< Local port to listen on any traffic, client or server. Not bound to any specific LWM2M functionality.*/
#define LWM2M_LOCAL_LISTENER_PORT             9997                                            /**< Local port to listen on any traffic. Bound to specific LWM2M functionality. */
#if APP_USE_CONTABO
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     5784                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5784                                            /**< Remote port of the LWM2M bootstrap server. */
#else
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     9998                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5684                                            /**< Remote port of the LWM2M bootstrap server. */
#endif
#define LWM2M_LOCAL_CLIENT_PORT_OFFSET        9999                                            /**< Local port to connect to the LWM2M server. */
#define LWM2M_SERVER_REMORT_PORT              5684                                            /**< Remote port of the LWM2M server. */

#if APP_USE_CONTABO
#define BOOTSTRAP_URI                   "coaps://vmi36865.contabo.host:5784"                  /**< Server URI to the bootstrap server when using security (DTLS). */
#else
#define BOOTSTRAP_URI                   "coaps://xvzwcdpii.xdev.motive.com:5684"              /**< Server URI to the bootstrap server when using security (DTLS). */
#endif

#define APP_SEC_TAG_OFFSET              25

#define APP_BOOTSTRAP_SEC_TAG           APP_SEC_TAG_OFFSET                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#if APP_USE_CONTABO
#define APP_BOOTSTRAP_SEC_PSK           {'g', 'l', 'e', 'n', 'n', 's', 's', 'e', 'c', 'r', 'e', 't'}  /**< Pre-shared key used for bootstrap server in hex format. */
#else
#define APP_BOOTSTRAP_SEC_PSK           {0xd6, 0x16, 0x0c, 0x2e, \
                                         0x7c, 0x90, 0x39, 0x9e, \
                                         0xe7, 0xd2, 0x07, 0xa2, \
                                         0x26, 0x11, 0xe3, 0xd3, \
                                         0xa8, 0x72, 0x41, 0xb0, \
                                         0x46, 0x29, 0x76, 0xb9, \
                                         0x35, 0x34, 0x1d, 0x00, \
                                         0x0a, 0x91, 0xe7, 0x47} /**< Pre-shared key used for bootstrap server in hex format. */
#endif

static char m_app_bootstrap_psk[]  = APP_BOOTSTRAP_SEC_PSK;
static int  m_app_admin_apn_handle = -1;

#if CONFIG_DK_LIBRARY
#define APP_ERROR_CHECK(error_code) \
    do { \
        if (error_code != 0) { \
            LOG_ERR("Error: %lu", error_code); \
            while (1); \
        } \
    } while (0)
#define APP_ERROR_CHECK_BOOL(boolean_value) \
    do { \
        const uint32_t local_value = (boolean_value); \
        if (!local_value) { \
            LOG_ERR("BOOL check failure"); \
            while (1); \
        } \
    } while (0)
#else
#define APP_ERROR_CHECK(error_code) \
    do { \
        if (error_code != 0) { \
            LOG_ERR("Error: %lu", error_code); \
            while (1); \
        } \
    } while (0)
#define APP_ERROR_CHECK_BOOL(boolean_value) \
    do { \
        const uint32_t local_value = (boolean_value); \
        if (!local_value) { \
            LOG_ERR("BOOL check failure"); \
            while (1); \
        } \
    } while (0)
#endif

static lwm2m_server_config_t               m_server_conf[1+LWM2M_MAX_SERVERS];                /**< Server configuration structure. */
static lwm2m_client_identity_t             m_client_id;                                       /**< Client ID structure to hold the client's UUID. */

// Objects
static lwm2m_object_t                      m_bootstrap_server;                                /**< Named object to be used as callback object when bootstrap is completed. */

static char m_bootstrap_object_alias_name[] = "bs";                                           /**< Name of the bootstrap complete object. */

static coap_transport_handle_t             m_coap_transport = -1;                             /**< CoAP transport handle for the non bootstrap server. */
static coap_transport_handle_t             m_lwm2m_transport[1+LWM2M_MAX_SERVERS];            /**< CoAP transport handles for the secure servers. Obtained on @coap_security_setup. */

static volatile app_state_t m_app_state = APP_STATE_BOOTING;                             /**< Application state. Should be one of @ref app_state_t. */
static volatile uint16_t    m_server_instance;                                                /**< Server instance handled. */
static volatile bool        m_did_bootstrap;
static volatile bool        app_server_update_requested[1+LWM2M_MAX_SERVERS];

static char m_imei[16];
static char m_msisdn[16];

/* Structures for delayed work */
static struct k_delayed_work state_update_work;

static struct k_delayed_work connection_update_work[1+LWM2M_MAX_SERVERS];

/* Resolved server addresses */
#if APP_USE_CONTABO
static sa_family_t m_family_type[1+LWM2M_MAX_SERVERS] = { AF_INET, AF_INET, 0, AF_INET };     /**< Current IP versions, start using IPv6. */
#else
static sa_family_t m_family_type[1+LWM2M_MAX_SERVERS] = { AF_INET6, AF_INET6, 0, AF_INET6 };  /**< Current IP versions, start using IPv6. */
#endif
static struct sockaddr m_bs_remote_server;                                                    /**< Remote bootstrap server address to connect to. */

static struct sockaddr m_remote_server[1+LWM2M_MAX_SERVERS];                                  /**< Remote secure server address to connect to. */
static volatile uint32_t tick_count = 0;

static void app_teardown_admin_pdn(void);
static void app_misc_data_set_bootstrapped(uint8_t bootstrapped);
static void app_server_deregister(uint16_t instance_id);
static void app_server_disconnect(uint16_t instance_id);
static void app_provision_psk(int sec_tag, char * identity, uint8_t identity_len, char * psk, uint8_t psk_len);
static void app_provision_secret_keys(void);
static void app_disconnect(void);
static void app_wait_state_update(struct k_work *work);
static const char * app_uri_get(char * p_server_uri, uint16_t * p_port, bool * p_secure);

/** Functions available from shell access */
void app_request_server_update(uint16_t instance_id)
{
    app_server_update_requested[instance_id] = true;
}

app_state_t app_state_get(void)
{
    return m_app_state;
}

void app_state_set(app_state_t app_state)
{
    m_app_state = app_state;
}

char *app_imei_get(void)
{
    return m_imei;
}

char *app_msisdn_get(void)
{
    return m_msisdn;
}

bool app_did_bootstrap(void)
{
    return m_did_bootstrap;
}

uint16_t app_server_instance(void)
{
    return m_server_instance;
}

sa_family_t app_family_type_get(uint16_t instance_id)
{
    return m_family_type[instance_id];
}

int32_t app_state_update_delay(void)
{
    return k_delayed_work_remaining_get(&state_update_work);
}

void app_system_shutdown(void)
{
    app_disconnect();

    if (m_app_admin_apn_handle != -1) {
        app_teardown_admin_pdn();
    }

    lte_lc_power_off();

    m_app_state = APP_STATE_SHUTDOWN;

    LOG_INF("LTE link down");
}

void app_system_reset(void)
{
    app_system_shutdown();
    NVIC_SystemReset();
}

/**@brief Setup ADMIN PDN connection */
static void app_setup_admin_pdn(void)
{
    // Set up APN for Bootstrap and DM server.
    uint8_t class_apn_len = 0;
    char * apn_name = lwm2m_conn_mon_class_apn_get(2, &class_apn_len);
    char apn_name_zero_terminated[64];

    memcpy(apn_name_zero_terminated, apn_name, class_apn_len);
    apn_name_zero_terminated[class_apn_len] = '\0';

    m_app_admin_apn_handle = at_apn_setup_wait_for_ipv6(apn_name_zero_terminated);

    LOG_INF("APN setup: %s", log_strdup(apn_name_zero_terminated));
}

/**@brief Tear down ADMIN PDN connection. */
static void app_teardown_admin_pdn(void)
{
    pdn_disconnect(m_app_admin_apn_handle);
    m_app_admin_apn_handle = -1;
}

/**@brief Initialize IMEI and MSISDN to use in client id.
 *
 * Factory reset to start bootstrap if MSISDN is different than last start.
 */
static char * app_initialize_client_id(void)
{
    static char client_id[128];
    bool provision_bs_psk = false;

    (void)at_read_imei_and_msisdn(m_imei, sizeof(m_imei), m_msisdn, sizeof(m_msisdn));

    char last_used_msisdn[128];
    const char *p_msisdn;

    const char * p_debug_msisdn = app_debug_msisdn_get();
    if (p_debug_msisdn && p_debug_msisdn[0]) {
        p_msisdn = p_debug_msisdn;
    } else {
        p_msisdn = m_msisdn;
    }

    int32_t len = lwm2m_last_used_msisdn_get(last_used_msisdn, sizeof(last_used_msisdn));
    if (len > 0) {
        if (strlen(p_msisdn) > 0 && strcmp(p_msisdn, last_used_msisdn) != 0) {
            // MSISDN has changed, factory reset and initiate bootstrap.
            LOG_INF("Detected changed MSISDN: %s -> %s", log_strdup(last_used_msisdn), log_strdup(p_msisdn));
            app_bootstrap_reset();
            lwm2m_last_used_msisdn_set(p_msisdn, strlen(p_msisdn) + 1);
            provision_bs_psk = true;
        }
    } else {
        lwm2m_last_used_msisdn_set(p_msisdn, strlen(p_msisdn) + 1);
        provision_bs_psk = true;
    }

    snprintf(client_id, 128, "urn:imei-msisdn:%s-%s", m_imei, p_msisdn);

    if (provision_bs_psk) {
        lte_lc_offline();
        app_provision_psk(APP_BOOTSTRAP_SEC_TAG, client_id, strlen(client_id), m_app_bootstrap_psk, sizeof(m_app_bootstrap_psk));
        lte_lc_init_and_connect();
    }

    return client_id;
}

/**@brief Application implementation of the root handler interface.
 *
 * @details This function is not bound to any object or instance. It will be called from
 *          LWM2M upon an action on the root "/" URI path. During bootstrap it is expected
 *          to get a DELETE operation on this URI.
 */
uint32_t lwm2m_coap_handler_root(uint8_t op_code, coap_message_t * p_request)
{
    (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);

    // Delete any existing objects or instances if needed.

    return 0;
}

static void app_init_sockaddr_in(struct sockaddr *addr, sa_family_t ai_family, u16_t port)
{
    memset(addr, 0, sizeof(struct sockaddr));

    if (ai_family == AF_INET)
    {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;

        addr_in->sin_family = ai_family;
        addr_in->sin_port = htons(port);
    }
    else
    {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;

        addr_in6->sin6_family = ai_family;
        addr_in6->sin6_port = htons(port);
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
        LOG_ERR("Invalid server URI: %s", log_strdup(server_uri));
        return NULL;
    }

    char *sep = strchr(hostname, ':');
    if (sep) {
        *sep = '\0';
        *p_port = atoi(sep + 1);
    }

    return hostname;
}

static void app_printable_ip_address(struct sockaddr * addr, char * ip_buffer, size_t ip_buffer_len)
{
    switch (addr->sa_family) {
    case AF_INET:
    {
        u32_t val = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
        snprintf(ip_buffer, ip_buffer_len, "%u.%u.%u.%u",
                ((uint8_t *)&val)[0], ((uint8_t *)&val)[1], ((uint8_t *)&val)[2], ((uint8_t *)&val)[3]);
        break;
    }

    case AF_INET6:
    {
        size_t pos = 0;
        bool elided = false;

        // Poor man's elided IPv6 address print.
        for (u8_t i = 0; i < 16; i += 2) {
            u16_t val = (((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[i] << 8) +
                        (((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[i+1]);

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

static uint32_t app_resolve_server_uri(char            * server_uri,
                                       uint8_t           uri_len,
                                       struct sockaddr * addr,
                                       bool            * secure,
                                       sa_family_t       family_type,
                                       int               pdn_handle)
{
    // Create a string copy to null-terminate hostname within the server_uri.
    char * p_server_uri_val = k_malloc(uri_len + 1);
    strncpy(p_server_uri_val, server_uri, uri_len);
    p_server_uri_val[uri_len] = 0;

    uint16_t port;
    const char *hostname = app_uri_get(p_server_uri_val, &port, secure);

    if (hostname == NULL) {
        k_free (p_server_uri_val);
        return EINVAL;
    }

    struct addrinfo hints = {
        .ai_family = family_type,
        .ai_socktype = SOCK_DGRAM
    };

    // Structures that might be pointed to by APN hints.
    struct addrinfo apn_hints;
    char apn_name_zero_terminated[64];

    if (pdn_handle > -1)
    {
        uint8_t class_apn_len = 0;
        char * apn_name = lwm2m_conn_mon_class_apn_get(2, &class_apn_len);

        memcpy(apn_name_zero_terminated, apn_name, class_apn_len);
        apn_name_zero_terminated[class_apn_len] = '\0';

        apn_hints.ai_family    = AF_LTE;
        apn_hints.ai_socktype  = SOCK_MGMT;
        apn_hints.ai_protocol  = NPROTO_PDN;
        apn_hints.ai_canonname = apn_name_zero_terminated;

        hints.ai_next = &apn_hints;
    }

    LOG_INF("Doing DNS lookup using %s (APN %s)",
            (family_type == AF_INET6) ? "IPv6" : "IPv4",
            (pdn_handle > -1) ? log_strdup(apn_name_zero_terminated) : "default");

    struct addrinfo *result;
    int ret_val = -1;
    int cnt = 1;

    // TODO:
    //  getaddrinfo() currently returns a mix of GAI error codes and NRF error codes.
    //  22 = NRF_EINVAL is invalid argument, but may also indicate no address found in the DNS query response.
    //  60 = NRF_ETIMEDOUT is a timeout waiting for DNS query response.

    while (ret_val != 0 && ret_val != 22 && ret_val != 60 && cnt <= 5) {
        ret_val = getaddrinfo(hostname, NULL, &hints, &result);
        if (ret_val != 0 && ret_val != 22 && ret_val != 60 && cnt < 5) {
            k_sleep(K_SECONDS(cnt));
        }
        cnt++;
    }

    if (ret_val == 22 || ret_val == 60) {
        LOG_WRN("No %s address found for \"%s\"", (family_type == AF_INET6) ? "IPv6" : "IPv4", log_strdup(hostname));
        k_free (p_server_uri_val);
        return EINVAL;
    } else if (ret_val != 0) {
        LOG_ERR("Failed to lookup \"%s\": %d", log_strdup(hostname), ret_val);
        k_free (p_server_uri_val);
        return ret_val;
    }

    app_init_sockaddr_in(addr, result->ai_family, port);

    if (result->ai_family == AF_INET) {
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
    } else {
        memcpy(((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, ((struct sockaddr_in6 *)result->ai_addr)->sin6_addr.s6_addr, 16);
    }

    freeaddrinfo(result);
    k_free (p_server_uri_val);

#if CONFIG_LOG
    char ip_buffer[64];
    app_printable_ip_address(addr, ip_buffer, sizeof(ip_buffer));
    LOG_INF("DNS result: %s", log_strdup(ip_buffer));
#endif

    return 0;
}

/**@brief Helper function to parse the uri and save the remote to the LWM2M remote database. */
static uint32_t app_lwm2m_parse_uri_and_save_remote(uint16_t          short_server_id,
                                                    char            * server_uri,
                                                    uint8_t           uri_len,
                                                    bool            * secure,
                                                    struct sockaddr * p_remote)
{
    uint32_t err_code;

    // Use DNS to lookup the IP
    err_code = app_resolve_server_uri(server_uri, uri_len, p_remote, secure, m_family_type[0], m_app_admin_apn_handle);

    if (err_code == 0)
    {
        // Deregister the short_server_id in case it has been registered with a different address
        (void) lwm2m_remote_deregister(short_server_id);

        // Register the short_server_id
        err_code = lwm2m_remote_register(short_server_id, (struct sockaddr *)p_remote);
    }

    return err_code;
}

/**@brief Helper function to handle a connect retry. */
void app_handle_connect_retry(int instance_id, bool no_reply)
{
    bool start_retry_delay = true;

    if (no_reply && !app_debug_flag_is_set(DEBUG_FLAG_DISABLE_IPv6))
    {
        // Fallback to the other IP version
        m_family_type[instance_id] = (m_family_type[instance_id] == AF_INET6) ? AF_INET : AF_INET6;

        if (m_family_type[instance_id] == AF_INET)
        {
            // No retry delay when IPv6 to IPv4 fallback
            LOG_INF("IPv6 to IPv4 fallback");
            start_retry_delay = false;
        }
    }

    if (start_retry_delay)
    {
        int32_t retry_delay = lwm2m_retry_delay_get(instance_id, true);

        if (retry_delay == -1) {
            LOG_ERR("Bootstrap procedure failed");
            m_app_state = APP_STATE_IP_INTERFACE_UP;
            lwm2m_retry_delay_reset(instance_id);
            return;
        }

        LOG_INF("Retry delay for %ld minutes (server %u)", retry_delay / 60, instance_id);
        k_delayed_work_submit(&state_update_work, retry_delay * 1000);
    } else {
        k_delayed_work_submit(&state_update_work, 0);
    }
}

static void app_restart_lifetime_timer(uint8_t instance_id)
{
    int32_t timeout = (int32_t)(lwm2m_server_lifetime_get(instance_id) * 1000);
    if (timeout <= 0) {
        // FIXME: Lifetime timer too big for Zephyr, set to maximum possible value for now
        timeout = INT32_MAX;
    }

    k_delayed_work_submit(&connection_update_work[instance_id], timeout);
}

/**@brief LWM2M notification handler. */
void lwm2m_notification(lwm2m_notification_type_t type,
                        struct sockaddr *         p_remote,
                        uint8_t                   coap_code,
                        uint32_t                  err_code)
{
#if CONFIG_LOG
    static char *str_type[] = { "Bootstrap", "Register", "Update", "Deregister" };
    LOG_INF("Got LWM2M notification %s  CoAP %d.%02d  err:%lu", str_type[type], coap_code >> 5, coap_code & 0x1f, err_code);
#endif

    if (type == LWM2M_NOTIFCATION_TYPE_BOOTSTRAP)
    {
        if (coap_code == COAP_CODE_204_CHANGED)
        {
            m_app_state = APP_STATE_BOOTSTRAPPING;
            LOG_INF("Bootstrap timeout set to 20 seconds");
            k_delayed_work_submit(&state_update_work, 20 * 1000);
        }
        else if (coap_code == 0 || coap_code == COAP_CODE_403_FORBIDDEN)
        {
            // No response or received a 4.03 error.
            m_app_state = APP_STATE_BOOTSTRAP_WAIT;
            app_handle_connect_retry(0, false);
        }
        else
        {
            // TODO: What to do here?
        }
        return;
    }

    uint16_t instance_id = 0xFFFF;
    uint16_t short_server_id = 0;

    if (p_remote == NULL) {
        LOG_WRN("Remote address missing");
        return;
    }
    else if (lwm2m_remote_short_server_id_find(&short_server_id, p_remote) != 0)
    {
        LOG_WRN("Remote address not found");
        return;
    }

    // Find the server instance for the short server ID.
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (lwm2m_server_short_server_id_get(i) == short_server_id) {
            instance_id = i;
            break;
        }
    }

    if (instance_id == 0xFFFF) {
        LOG_WRN("Server instance for short server ID not found: %d", short_server_id);
        return;
    }

    if (type == LWM2M_NOTIFCATION_TYPE_REGISTER)
    {
        app_restart_lifetime_timer(instance_id);

        if (coap_code == COAP_CODE_201_CREATED || coap_code == COAP_CODE_204_CHANGED)
        {
            LOG_INF("Registered (server %u)", instance_id);
            lwm2m_retry_delay_reset(instance_id);
            lwm2m_server_registered_set(instance_id, true);

            m_app_state = APP_STATE_IDLE;
        }
        else
        {
            m_app_state = APP_STATE_SERVER_REGISTER_WAIT;
            app_handle_connect_retry(instance_id, false);
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_UPDATE)
    {
        if (coap_code == 0) {
            // No response from update request
            LOG_INF("Reconnect (server %d)", instance_id);
            app_server_disconnect(instance_id);
            app_request_server_update(instance_id);
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_DEREGISTER)
    {
        // We have successfully deregistered.
        lwm2m_server_registered_set(instance_id, false);

        if (m_app_state == APP_STATE_SERVER_DEREGISTERING)
        {
            LOG_INF("Deregistered (server %d)", instance_id);
            uint8_t uri_len = 0;
            for (int i = instance_id-1; i > 0; i--) {
                if (m_lwm2m_transport[i] != -1) {
                    // Only deregister from connected servers having a URI.
                    (void)lwm2m_security_server_uri_get(i, &uri_len);
                    if (uri_len > 0) {
                        m_app_state = APP_STATE_SERVER_DEREGISTER;
                        m_server_instance = i;
                        break;
                    }
                }
            }

            if (uri_len == 0) {
                // No more servers to deregister
                m_app_state = APP_STATE_DISCONNECT;
            }
        }
        else
        {
            int32_t delay = (int32_t) lwm2m_server_disable_timeout_get(instance_id);
            LOG_INF("Disable [%ld seconds] (server %d)", delay, instance_id);
            app_server_disconnect(instance_id);

            k_delayed_work_submit(&connection_update_work[instance_id], delay * 1000);
        }
    }
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

void init_request_server_update(void)
{
    // Register all servers having a URI.
    for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++) {
        uint8_t uri_len = 0;
        (void)lwm2m_security_server_uri_get(i, &uri_len);
        if (uri_len > 0) {
            app_request_server_update(i);
        }
    }
}

/**@brief Callback function for the named bootstrap complete object. */
uint32_t bootstrap_object_callback(lwm2m_object_t * p_object,
                                   uint16_t         instance_id,
                                   uint8_t          op_code,
                                   coap_message_t * p_request)
{
    s64_t time_stamp;
    s64_t milliseconds_spent;

    LOG_INF("Bootstrap done, timeout cancelled");
    k_delayed_work_cancel(&state_update_work);

    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    k_sleep(10); // TODO: figure out why this is needed before closing the connection

    // Close connection to bootstrap server.
    app_server_disconnect(0);
    lwm2m_retry_delay_reset(0);

    time_stamp = k_uptime_get();

    app_provision_secret_keys();

    lwm2m_security_bootstrapped_set(0, true);  // TODO: this should be set by bootstrap server when bootstrapped
    m_did_bootstrap = true;

    // Clean bootstrap.
    app_misc_data_set_bootstrapped(1);

    LOG_INF("Store bootstrap settings");
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        lwm2m_instance_storage_security_store(i);
        lwm2m_instance_storage_server_store(i);
    }

    init_request_server_update();

    milliseconds_spent = k_uptime_delta(&time_stamp);
#if APP_USE_CONTABO
    // On contabo we want to jump directly to start connecting to servers when bootstrap is complete.
    m_app_state = APP_STATE_SERVER_CONNECT;
    m_server_instance = 1;
#else
    m_app_state = APP_STATE_BOOTSTRAP_HOLDOFF;
    int32_t hold_off_time = (lwm2m_server_client_hold_off_timer_get(0) * 1000) - milliseconds_spent;
    if (hold_off_time > 0) {
        LOG_INF("Client holdoff timer: sleeping %ld milliseconds...", hold_off_time);
    } else {
        // Already used the holdoff timer, just continue
        hold_off_time = 0;
    }
    k_delayed_work_submit(&state_update_work, hold_off_time);
#endif

    return 0;
}

static void app_init_server_acl(uint16_t instance_id, lwm2m_instance_acl_t *acl)
{
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    // Initialize ACL on the instance.
    (void)lwm2m_acl_permissions_init(p_instance, acl->owner);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add(p_instance,
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    for (uint32_t j = 0; j < ARRAY_SIZE(acl->server); j++)
    {
        if (acl->server[j] != 0)
        {
            // Set server access.
            (void)lwm2m_acl_permissions_add(p_instance, acl->access[j], acl->server[j]);
        }
    }
}

/**@brief Create factory bootstrapped server objects.
 *        Depends on carrier, this is Verizon / MotiveBridge.
 */
static void app_factory_bootstrap_server_object(uint16_t instance_id)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    lwm2m_instance_acl_t acl = {
        .owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID
    };

    switch (instance_id)
    {
        case 0: // Bootstrap server
        {
            lwm2m_server_short_server_id_set(0, 100);
            lwm2m_server_client_hold_off_timer_set(0, 30);

            lwm2m_security_server_uri_set(0, BOOTSTRAP_URI, strlen(BOOTSTRAP_URI));
            lwm2m_security_is_bootstrap_server_set(0, true);
            lwm2m_security_bootstrapped_set(0, false);

            acl.access[0] = rwde_access;
            acl.server[0] = 102;
            app_init_server_acl(0, &acl);
            break;
        }

        case 1: // DM server
        {
            acl.access[0] = rwde_access;
            acl.server[0] = 101;
            acl.access[1] = rwde_access;
            acl.server[1] = 102;
            acl.access[2] = rwde_access;
            acl.server[2] = 1000;
            app_init_server_acl(1, &acl);
            break;
        }

        case 2: // Diagnostics server
        {
            lwm2m_server_short_server_id_set(2, 101);
            lwm2m_server_client_hold_off_timer_set(2, 30);

            lwm2m_security_server_uri_set(2, "", 0);
            lwm2m_server_lifetime_set(2, 86400);
            lwm2m_server_min_period_set(2, 300);
            lwm2m_server_min_period_set(2, 6000);
            lwm2m_server_notif_storing_set(2, 1);
            lwm2m_server_binding_set(2, "UQS", 3);

            acl.access[0] = rwde_access;
            acl.server[0] = 102;
            app_init_server_acl(2, &acl);
            break;
        }

        case 3: // Repository server
        {
            acl.access[0] = rwde_access;
            acl.server[0] = 101;
            acl.access[1] = rwde_access;
            acl.server[1] = 102;
            acl.access[2] = rwde_access;
            acl.server[2] = 1000;
            app_init_server_acl(3, &acl);
            break;
        }

        default:
            break;
    }
}

static void app_misc_data_set_bootstrapped(uint8_t bootstrapped)
{
    lwm2m_instance_storage_misc_data_t misc_data = { 0 };
    lwm2m_instance_storage_misc_data_load(&misc_data);
    misc_data.bootstrapped = bootstrapped;
    lwm2m_instance_storage_misc_data_store(&misc_data);
}

void app_bootstrap_reset(void)
{
    if (lwm2m_security_short_server_id_get(0) == 0) {
        // Server object not loaded yet
        lwm2m_instance_storage_security_load(0);
    }

    if (lwm2m_security_bootstrapped_get(0)) {
        // Security object exists and bootstrap is done
        lwm2m_security_bootstrapped_set(0, false);
        lwm2m_instance_storage_security_store(0);
    }

    app_misc_data_set_bootstrapped(0);

    for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++) {
        lwm2m_instance_storage_security_delete(i);
        lwm2m_instance_storage_server_delete(i);
        // Set server short_id to 0 to disable
        lwm2m_server_short_server_id_set(i, 0);
    }
}

void app_factory_reset(void)
{
    app_misc_data_set_bootstrapped(0);

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_storage_security_delete(i);
        lwm2m_instance_storage_server_delete(i);
    }
}

static void app_load_flash_objects(void)
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
        lwm2m_instance_storage_security_load(i);
        lwm2m_instance_storage_server_load(i);

        if (lwm2m_server_short_server_id_get(i) == 0)
        {
            // Instance not loaded from flash, init factory defaults
            app_factory_bootstrap_server_object(i);

            if (lwm2m_server_short_server_id_get(i) > 0)
            {
                lwm2m_instance_storage_security_store(i);
                lwm2m_instance_storage_server_store(i);
            }
        }
    }

    lwm2m_instance_storage_misc_data_t misc_data;
    int32_t result = lwm2m_instance_storage_misc_data_load(&misc_data);
    if (result == 0 && misc_data.bootstrapped)
    {
        lwm2m_security_bootstrapped_set(0, true);
    }
    else
    {
        // storage reports that bootstrap has not been done, continue with bootstrap.
        lwm2m_security_bootstrapped_set(0, false);
    }
}

static void app_lwm2m_create_objects(void)
{
    lwm2m_security_init();
    lwm2m_server_init();

    // Initialize security, server and acl from flash.
    app_load_flash_objects();

    lwm2m_device_init();
    lwm2m_conn_mon_init();
    lwm2m_firmware_init();
}

/**@brief LWM2M initialization.
 *
 * @details The function will register all implemented base objects as well as initial registration
 *          of existing instances. If bootstrap is not performed, the registration to the server
 *          will use what is initialized in this function.
 */
static void app_lwm2m_setup(void)
{
    (void)lwm2m_init(k_malloc, k_free);
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

    // Initialize client ID.
    char * p_ep_id = app_initialize_client_id();

    memcpy(&m_client_id.value.imei_msisdn[0], p_ep_id, strlen(p_ep_id));
    m_client_id.len  = strlen(p_ep_id);
    m_client_id.type = LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN;
}

static void app_bootstrap_connect(void)
{
    uint32_t err_code;
    bool secure;

    // Save the remote address of the bootstrap server.
    uint8_t uri_len = 0;
    char * p_server_uri = lwm2m_security_server_uri_get(0, &uri_len);
    err_code = app_lwm2m_parse_uri_and_save_remote(LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID,
                                                   p_server_uri,
                                                   uri_len,
                                                   &secure,
                                                   &m_bs_remote_server);
    if (err_code != 0) {
        m_app_state = APP_STATE_BS_CONNECT_RETRY_WAIT;
        if (err_code == EINVAL) {
            app_handle_connect_retry(0, true);
        } else {
            app_handle_connect_retry(0, false);
        }
        return;
    }

    if (secure == true)
    {
        LOG_DBG("SECURE session (bootstrap)");

        struct sockaddr local_addr;
        app_init_sockaddr_in(&local_addr, m_bs_remote_server.sa_family, LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT);

        #define SEC_TAG_COUNT 1

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = {APP_BOOTSTRAP_SEC_TAG};

        coap_sec_config_t setting =
        {
            .role          = 0,    // 0 -> Client role
            .sec_tag_count = SEC_TAG_COUNT,
            .sec_tag_list  = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr         = &local_addr,
            .setting      = &setting,
            .protocol     = IPPROTO_DTLS_1_2
        };

        char apn_name_zero_terminated[64];
        if (m_app_admin_apn_handle != -1)
        {
            uint8_t class_apn_len = 0;
            char * apn_name = lwm2m_conn_mon_class_apn_get(2, &class_apn_len);

            memcpy(apn_name_zero_terminated, apn_name, class_apn_len);
            apn_name_zero_terminated[class_apn_len] = '\0';

            local_port.interface = apn_name_zero_terminated;
        }

        LOG_INF("Setup secure DTLS session (server 0) (APN %s)",
                (local_port.interface) ? log_strdup(apn_name_zero_terminated) : "default");

        err_code = coap_security_setup(&local_port, &m_bs_remote_server);

        if (err_code == 0)
        {
            LOG_INF("Connected");
            m_app_state = APP_STATE_BS_CONNECTED;
            m_lwm2m_transport[0] = local_port.transport;
        }
        else if (err_code == EINPROGRESS)
        {
            m_app_state = APP_STATE_BS_CONNECT_WAIT;
            m_lwm2m_transport[0] = local_port.transport;
        }
        else
        {
            LOG_INF("Connection failed: %ld (%d)", err_code, errno);
            m_app_state = APP_STATE_BS_CONNECT_RETRY_WAIT;
            // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
            if (err_code == EIO && (errno == EINVAL || errno == EOPNOTSUPP || errno == ENETUNREACH)) {
                app_handle_connect_retry(0, true);
            } else {
                app_handle_connect_retry(0, false);
            }
        }
    }
    else
    {
        LOG_DBG("NON-SECURE session (bootstrap)");
        m_app_state = APP_STATE_BS_CONNECTED;
    }
}

static void app_bootstrap(void)
{
    uint32_t err_code = lwm2m_bootstrap((struct sockaddr *)&m_bs_remote_server,
                                        &m_client_id,
                                        m_lwm2m_transport[0]);
    if (err_code == 0)
    {
        m_app_state = APP_STATE_BOOTSTRAP_REQUESTED;
    }
}

static void app_server_connect(uint16_t instance_id)
{
    uint32_t err_code;
    bool secure;

    // Initialize server configuration structure.
    memset(&m_server_conf[instance_id], 0, sizeof(lwm2m_server_config_t));
    m_server_conf[instance_id].lifetime = lwm2m_server_lifetime_get(instance_id);

    if (app_debug_flag_is_set(DEBUG_FLAG_SMS_SUPPORT)) {
        m_server_conf[instance_id].binding.p_val = "UQS";
        m_server_conf[instance_id].binding.len = 3;

        if (instance_id) {
            char *endptr;
            const char * p_msisdn = app_debug_msisdn_get();
            if (!p_msisdn || p_msisdn[0] == 0) {
                p_msisdn = m_msisdn;
            }

            m_server_conf[instance_id].msisdn = strtoull(p_msisdn, &endptr, 10);
        }
    }

    // Set the short server id of the server in the config.
    m_server_conf[instance_id].short_server_id = lwm2m_server_short_server_id_get(instance_id);

    uint8_t uri_len = 0;
    char * p_server_uri = lwm2m_security_server_uri_get(instance_id, &uri_len);
    int apn_handle = -1;

    if (instance_id == 1) {
        apn_handle = m_app_admin_apn_handle;
    }

    err_code = app_resolve_server_uri(p_server_uri,
                                      uri_len,
                                      &m_remote_server[instance_id],
                                      &secure,
                                      m_family_type[instance_id],
                                      apn_handle);
    if (err_code != 0)
    {
        m_app_state = APP_STATE_SERVER_CONNECT_RETRY_WAIT;
        if (err_code == EINVAL) {
            app_handle_connect_retry(instance_id, true);
        } else {
            app_handle_connect_retry(instance_id, false);
        }
        return;
    }

    if (secure == true)
    {
        LOG_DBG("SECURE session (register)");

        // TODO: Check if this has to be static.
        struct sockaddr local_addr;
        app_init_sockaddr_in(&local_addr, m_remote_server[instance_id].sa_family, LWM2M_LOCAL_CLIENT_PORT_OFFSET + instance_id);

        #define SEC_TAG_COUNT 1

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = { APP_SEC_TAG_OFFSET + instance_id };

        coap_sec_config_t setting =
        {
            .role          = 0,    // 0 -> Client role
            .sec_tag_count = SEC_TAG_COUNT,
            .sec_tag_list  = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr         = &local_addr,
            .setting      = &setting,
            .protocol     = IPPROTO_DTLS_1_2
        };

        char apn_name_zero_terminated[64];
        if ((instance_id == 1) && (m_app_admin_apn_handle != -1))
        {
            uint8_t class_apn_len = 0;
            char * apn_name = lwm2m_conn_mon_class_apn_get(2, &class_apn_len);

            memcpy(apn_name_zero_terminated, apn_name, class_apn_len);
            apn_name_zero_terminated[class_apn_len] = '\0';

            local_port.interface = apn_name_zero_terminated;
        }

        LOG_INF("Setup secure DTLS session (server %u) (APN %s)",
                instance_id,
                (local_port.interface) ? log_strdup(apn_name_zero_terminated) : "default");

        err_code = coap_security_setup(&local_port, &m_remote_server[instance_id]);

        if (err_code == 0)
        {
            LOG_INF("Connected");
            m_app_state = APP_STATE_SERVER_CONNECTED;
            m_lwm2m_transport[instance_id] = local_port.transport;
        }
        else if (err_code == EINPROGRESS)
        {
            m_app_state = APP_STATE_SERVER_CONNECT_WAIT;
            m_lwm2m_transport[instance_id] = local_port.transport;
        }
        else
        {
            LOG_INF("Connection failed: %ld (%d)", err_code, errno);
            m_app_state = APP_STATE_SERVER_CONNECT_RETRY_WAIT;
            // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
            if (err_code == EIO && (errno == EINVAL || errno == EOPNOTSUPP || errno == ENETUNREACH)) {
                app_handle_connect_retry(instance_id, true);
            } else {
                app_handle_connect_retry(instance_id, false);
            }
        }
    }
    else
    {
        LOG_DBG("NON-SECURE session (register)");
        m_app_state = APP_STATE_SERVER_CONNECTED;
    }
}

static void app_server_register(uint16_t instance_id)
{
    uint32_t err_code;
    uint32_t link_format_string_len = 0;

    // Dry run the link format generation, to check how much memory that is needed.
    err_code = lwm2m_coap_handler_gen_link_format(NULL, (uint16_t *)&link_format_string_len);
    APP_ERROR_CHECK(err_code);

    // Allocate the needed amount of memory.
    uint8_t * p_link_format_string = k_malloc(link_format_string_len);

    if (p_link_format_string != NULL)
    {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(p_link_format_string, (uint16_t *)&link_format_string_len);
        APP_ERROR_CHECK(err_code);

        err_code = lwm2m_register((struct sockaddr *)&m_remote_server[instance_id],
                                  &m_client_id,
                                  &m_server_conf[instance_id],
                                  m_lwm2m_transport[instance_id],
                                  p_link_format_string,
                                  (uint16_t)link_format_string_len);
        APP_ERROR_CHECK(err_code);

        m_app_state = APP_STATE_SERVER_REGISTER_WAIT;
        k_free(p_link_format_string);
    }
}

void app_server_update(uint16_t instance_id)
{
    if ((m_app_state == APP_STATE_IDLE) ||
        (instance_id != m_server_instance))
    {
        uint32_t err_code;

        err_code = lwm2m_update((struct sockaddr *)&m_remote_server[instance_id],
                                &m_server_conf[instance_id],
                                m_lwm2m_transport[instance_id]);
        ARG_UNUSED(err_code);
    }
    else
    {
        LOG_WRN("Unable to do server update (server %u)", instance_id);
    }

    app_restart_lifetime_timer(instance_id);
}

void app_server_disable(uint16_t instance_id)
{
    uint32_t err_code;

    err_code = lwm2m_deregister((struct sockaddr *)&m_remote_server[instance_id],
                                m_lwm2m_transport[instance_id]);
    APP_ERROR_CHECK(err_code);
}

static void app_server_deregister(uint16_t instance_id)
{
    uint32_t err_code;

    err_code = lwm2m_deregister((struct sockaddr *)&m_remote_server[instance_id],
                                m_lwm2m_transport[instance_id]);
    APP_ERROR_CHECK(err_code);

    m_app_state = APP_STATE_SERVER_DEREGISTERING;
}

static void app_server_disconnect(uint16_t instance_id)
{
    if (m_lwm2m_transport[instance_id] != -1) {
        coap_security_destroy(m_lwm2m_transport[instance_id]);
        m_lwm2m_transport[instance_id] = -1;
    }
}

static void app_disconnect(void)
{
    // Destroy the secure session if any.
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        app_server_disconnect(i);
    }

    m_app_state = APP_STATE_IP_INTERFACE_UP;
}

static void app_wait_state_update(struct k_work *work)
{
    ARG_UNUSED(work);

    switch (m_app_state)
    {
        case APP_STATE_BS_CONNECT_RETRY_WAIT:
            // Timeout waiting for DTLS connection to bootstrap server
            m_app_state = APP_STATE_BS_CONNECT;
            break;

        case APP_STATE_BOOTSTRAP_WAIT:
            // Timeout waiting for bootstrap ACK (CoAP)
            m_app_state = APP_STATE_BS_CONNECTED;
            break;

        case APP_STATE_BOOTSTRAPPING:
            // Timeout waiting for bootstrap to finish
            m_app_state = APP_STATE_BOOTSTRAP_TIMEDOUT;
            break;

        case APP_STATE_BOOTSTRAP_HOLDOFF:
            // Client holdoff timer expired
            m_app_state = APP_STATE_IDLE;
            break;

        case APP_STATE_SERVER_CONNECT_RETRY_WAIT:
            // Timeout waiting for DTLS connection to registration server
            m_app_state = APP_STATE_SERVER_CONNECT;
            break;

        case APP_STATE_SERVER_REGISTER_WAIT:
            // Timeout waiting for registration ACK (CoAP)
            m_app_state = APP_STATE_SERVER_CONNECTED;
            break;

        default:
            // Unknown timeout state
            break;
    }
}

#if APP_USE_SOCKET_POLL
static bool app_coap_socket_poll(void)
{
    struct pollfd fds[1+LWM2M_MAX_SERVERS];
    int nfds = 0;
    int ret = 0;

    // Find active sockets
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (m_lwm2m_transport[i] != -1) {
            fds[nfds].fd = m_lwm2m_transport[i];
            fds[nfds].events = POLLIN;

            // Only check POLLOUT (writing possible) when waiting for connect()
            if ((i == m_server_instance) &&
                ((m_app_state == APP_STATE_BS_CONNECT_WAIT) ||
                 (m_app_state == APP_STATE_SERVER_CONNECT_WAIT))) {
                fds[nfds].events |= POLLOUT;
            }
            nfds++;
        }
    }

    if (nfds > 0) {
        ret = poll(fds, nfds, 1000);
    } else {
        // No active sockets to poll.
        k_sleep(1000);
    }

    if (ret == 0) {
        // Timeout; nothing more to check.
        return false;
    } else if (ret < 0) {
        printk("poll error: %d", errno);
        return false;
    }

    bool data_ready = false;

    for (int i = 0; i < nfds; i++) {
        if ((fds[i].revents & POLLIN) == POLLIN) {
            // There is data to read.
            data_ready = true;
        }

        if ((fds[i].revents & POLLOUT) == POLLOUT) {
            // Writing is now possible.
            if (m_app_state == APP_STATE_BS_CONNECT_WAIT) {
                LOG_INF("Connected");
                m_app_state = APP_STATE_BS_CONNECTED;
            } else if (m_app_state == APP_STATE_SERVER_CONNECT_WAIT) {
                LOG_INF("Connected");
                m_app_state = APP_STATE_SERVER_CONNECTED;
            }
        }

        if ((fds[i].revents & POLLERR) == POLLERR) {
            // Error condition.
            if (m_app_state == APP_STATE_BS_CONNECT_WAIT) {
                m_app_state = APP_STATE_BS_CONNECT_RETRY_WAIT;
            } else if (m_app_state == APP_STATE_SERVER_CONNECT_WAIT) {
                m_app_state = APP_STATE_SERVER_CONNECT_RETRY_WAIT;
            } else {
                // TODO handle?
                printk("POLLERR: %d\n", i);
                continue;
            }

            int error = 0;
            int len = sizeof(error);
            getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len);

            uint32_t err_code = coap_security_destroy(fds[i].fd);
            ARG_UNUSED(err_code);

            // NOTE: This works because we are only connecting to one server at a time.
            m_lwm2m_transport[m_server_instance] = -1;

            LOG_INF("Connection failed (%d)", error);

            // Check for no IPv6 support (EINVAL or EOPNOTSUPP) and no response (ENETUNREACH)
            if (error == EINVAL || error == EOPNOTSUPP || error == ENETUNREACH) {
                app_handle_connect_retry(m_server_instance, true);
            } else {
                app_handle_connect_retry(m_server_instance, false);
            }
        }

        if ((fds[i].revents & POLLNVAL) == POLLNVAL) {
            // TODO: Ignore POLLNVAL for now.
            printk("POLLNVAL: %d\n", i);
        }
    }

    return data_ready;
}
#endif

static void app_check_server_update(void)
{
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (app_server_update_requested[i]) {
            if (m_lwm2m_transport[i] == -1) {
                if (m_app_state == APP_STATE_IDLE) {
                    m_app_state = APP_STATE_SERVER_CONNECT;
                    m_server_instance = i;
                    app_server_update_requested[i] = false;
                }
            } else if (lwm2m_server_registered_get(i)) {
                LOG_INF("app_server_update (server %u)", i);
                app_server_update(i);
                app_server_update_requested[i] = false;
            }
        }
    }
}

static void app_lwm2m_process(void)
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
        case APP_STATE_BS_CONNECT:
        {
            LOG_INF("app_bootstrap_connect");
            app_bootstrap_connect();
            break;
        }
        case APP_STATE_BOOTSTRAP_TIMEDOUT:
        {
            LOG_INF("app_handle_connect_retry");
            app_disconnect();
            m_app_state = APP_STATE_BS_CONNECT_RETRY_WAIT;
            app_handle_connect_retry(0, false);
            break;
        }
        case APP_STATE_BS_CONNECTED:
        {
            LOG_INF("app_bootstrap");
            app_bootstrap();
            break;
        }
        case APP_STATE_SERVER_CONNECT:
        {
            LOG_INF("app_server_connect (server %u)", m_server_instance);
            app_server_connect(m_server_instance);
            break;
        }
        case APP_STATE_SERVER_CONNECTED:
        {
            LOG_INF("app_server_register (server %u)", m_server_instance);
            app_server_register(m_server_instance);
            break;
        }
        case APP_STATE_SERVER_DEREGISTER:
        {
            LOG_INF("app_server_deregister (server %u)", m_server_instance);
            app_server_deregister(m_server_instance);
            break;
        }
        case APP_STATE_DISCONNECT:
        {
            LOG_INF("app_disconnect");
            app_disconnect();
            break;
        }
        default:
        {
            break;
        }
    }

    app_check_server_update();
}

static void app_coap_init(void)
{
    uint32_t err_code;

    struct sockaddr local_addr;
    struct sockaddr non_sec_local_addr;
    app_init_sockaddr_in(&local_addr, AF_INET, COAP_LOCAL_LISTENER_PORT);
    app_init_sockaddr_in(&non_sec_local_addr, m_family_type[1], LWM2M_LOCAL_LISTENER_PORT);

    // If bootstrap server and server is using different port we can
    // register the ports individually.
    coap_local_t local_port_list[COAP_PORT_COUNT] =
    {
        {
            .addr = &local_addr
        },
        {
            .addr = &non_sec_local_addr,
            .protocol = IPPROTO_UDP,
            .setting = NULL,
        }
    };

    // Verify that the port count defined in sdk_config.h is matching the one configured for coap_init.
    APP_ERROR_CHECK_BOOL(((sizeof(local_port_list)) / (sizeof(coap_local_t))) == COAP_PORT_COUNT);

    coap_transport_init_t port_list;
    port_list.port_table = &local_port_list[0];

    // Fetch last used CoAP Message ID
    lwm2m_instance_storage_misc_data_t misc_data = { 0 };
    lwm2m_instance_storage_misc_data_load(&misc_data);

    err_code = coap_init(misc_data.coap_initial_message_id, &port_list, k_malloc, k_free);
    APP_ERROR_CHECK(err_code);

    misc_data.coap_initial_message_id += 100;
    (void)lwm2m_instance_storage_misc_data_store(&misc_data);

    m_coap_transport = local_port_list[0].transport;
    ARG_UNUSED(m_coap_transport);

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        m_lwm2m_transport[i] = -1;
    }

#if APP_USE_CONTABO
    m_lwm2m_transport[1] = local_port_list[1].transport;
    ARG_UNUSED(m_lwm2m_transport);
#endif
}

static void app_provision_psk(int sec_tag, char * identity, uint8_t identity_len, char * psk, uint8_t psk_len)
{
    uint32_t err_code;

    err_code = nrf_inbuilt_key_write(sec_tag,
                                     NRF_KEY_MGMT_CRED_TYPE_IDENTITY,
                                     identity, identity_len);
    APP_ERROR_CHECK(err_code);

    size_t secret_key_nrf9160_style_len = psk_len * 2;
    uint8_t * p_secret_key_nrf9160_style = k_malloc(secret_key_nrf9160_style_len);
    for (int i = 0; i < psk_len; i++)
    {
        sprintf(&p_secret_key_nrf9160_style[i * 2], "%02x", psk[i]);
    }
    err_code = nrf_inbuilt_key_write(sec_tag,
                                     NRF_KEY_MGMT_CRED_TYPE_PSK,
                                     p_secret_key_nrf9160_style, secret_key_nrf9160_style_len);
    k_free(p_secret_key_nrf9160_style);
    APP_ERROR_CHECK(err_code);
}

static void app_provision_secret_keys(void)
{
    if (m_app_admin_apn_handle != -1)
    {
        app_teardown_admin_pdn();
    }

    lte_lc_offline();
    LOG_DBG("Offline mode");

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
            char * p_server_uri_val = k_malloc(uri_len + 1);

            strncpy(p_server_uri_val, p_server_uri, uri_len);
            p_server_uri_val[uri_len] = 0;

            bool secure = false;
            uint16_t port = 0;
            const char * hostname = app_uri_get(p_server_uri_val, &port, &secure);
            ARG_UNUSED(hostname);

            k_free(p_server_uri_val);

            if (secure) {
                LOG_DBG("Provisioning key for %s, short-id: %u", log_strdup(p_server_uri_val), lwm2m_server_short_server_id_get(i));
                app_provision_psk(APP_SEC_TAG_OFFSET + i, p_identity, identity_len, p_psk, psk_len);
            }
        }
    }
    LOG_INF("Wrote secret keys");

    lte_lc_init_and_connect();

    if (app_debug_flag_is_set(DEBUG_FLAG_PDN_SUPPORT)) {
        app_setup_admin_pdn();
    }

    // THIS IS A HACK. Temporary solution to give a delay to recover Non-DTLS sockets from CFUN=4.
    // The delay will make TX available after CID again set.
    k_sleep(K_MSEC(2000));

    LOG_DBG("Normal mode");
}

/**@brief Handle server lifetime.
 */
static void app_connection_update(struct k_work *work)
{
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (work == (struct k_work *)&connection_update_work[i]) {
            app_request_server_update(i);
            break;
        }
    }
}

/**@brief Initializes and submits delayed work. */
static void app_work_init(void)
{
    k_delayed_work_init(&connection_update_work[1], app_connection_update);
    k_delayed_work_init(&connection_update_work[3], app_connection_update);
    k_delayed_work_init(&state_update_work, app_wait_state_update);
}

static void app_lwm2m_observer_process(void)
{
    lwm2m_server_observer_process();
    lwm2m_conn_mon_observer_process();
    lwm2m_firmware_observer_process();
}

int lwm2m_vzw_init(void)
{
    // Initialize Non-volatile Storage.
    lwm2m_instance_storage_init();

    // Initialize debug settings from flash.
    app_debug_init();

    // Enable logging before establing LTE link.
    app_debug_modem_logging_enable();

    // Establish LTE link.
    if (app_debug_flag_is_set(DEBUG_FLAG_DISABLE_PSM)) {
        lte_lc_psm_req(false);
    }

    lte_lc_init_and_connect();

    if (app_debug_flag_is_set(DEBUG_FLAG_DISABLE_IPv6)) {
        for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
            m_family_type[i] = AF_INET;
        }
    }

    // Initialize CoAP.
    app_coap_init();

    // Setup LWM2M endpoints.
    app_lwm2m_setup();

    // Create LwM2M factory bootstraped objects.
    app_lwm2m_create_objects();

    if (app_debug_flag_is_set(DEBUG_FLAG_PDN_SUPPORT)) {
        LOG_INF("PDN support enabled");
        app_setup_admin_pdn();
    } else {
        LOG_INF("PDN support disabled");
    }

    app_work_init();

    if (lwm2m_security_bootstrapped_get(0)) {
        m_app_state = APP_STATE_IDLE;
        init_request_server_update();
    } else {
        m_app_state = APP_STATE_BS_CONNECT;
    }

    return 0;
}

void lwm2m_vzw_run(void)
{
    for (;;)
    {
        if (IS_ENABLED(CONFIG_LOG)) {
            /* if logging is enabled, sleep */
            k_sleep(K_MSEC(10));
        } else {
            /* other, put CPU to idle to save power */
            k_cpu_idle();
        }

        if (tick_count++ % 100 == 0) {
            // Pass a tick to CoAP in order to re-transmit any pending messages.
            ARG_UNUSED(coap_time_tick());
        }

        app_lwm2m_process();

        if (tick_count % 1000 == 0)
        {
            app_lwm2m_observer_process();
        }
    }
}
