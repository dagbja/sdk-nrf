/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m_api.h>                 // lwm2m_server_config_t
#include <coap_transport.h>            // coap_transport_handle_t
#include <nrf_socket.h>                // nrf_sockaddr_in6
#include <nrf_errno.h>                 // NRF_Exxx

#include <lwm2m.h>
#include <lwm2m_carrier_client.h>
#include <lwm2m_client_util.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_access_control.h>
#include <lwm2m_remote.h>
#include <lwm2m_conn_ext.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_pdn.h>
#include <lwm2m_apn_conn_prof.h>
#include <lwm2m_observer_storage.h>
#include <lwm2m_retry_delay.h>
#include <operator_check.h>

#include <lwm2m_carrier.h>
#include <lwm2m_carrier_main.h>

#if CONFIG_SHELL
#include <stdio.h>
#include <shell/shell.h>
#endif // CONFIG_SHELL

/*
 * We only need two client context instances because we only support two DTLS sessions.
 * This may also be used to optimise some LwM2M core internal storages.
 */

#define CLIENT_FLAG_WORK_Q_STARTED         0x01  // Set when workqueue is started
#define CLIENT_FLAG_SECURE_CONNECTION      0x02  // Set when using DTLS
#define CLIENT_FLAG_USE_HOLDOFF_TIMER      0x04  // Use client hold off timer after bootstrap
#define CLIENT_FLAG_CONNECTION_USE_APN     0x08  // Use APN for connection
#define CLIENT_FLAG_IP_FALLBACK_POSSIBLE   0x10  // Set if PDN having both IPv6 and IPv4
#define CLIENT_FLAG_IS_CONNECTING          0x20  // Set when doing connect()
#define CLIENT_FLAG_IS_REGISTERED          0x40  // Set when connected Registered

typedef struct {
    // Work handling
    struct k_work_q          work_q;             // Workqueue for tasks in this client context

    // Server context values
    nrf_sa_family_t          family_type;        // NRF_AF_INET or NRF_AF_INET6
    lwm2m_server_config_t    server_conf;        // LwM2M server configuration.
    struct nrf_sockaddr_in6  remote_server;      // Remote server address (IPv4 or IPv6).
    coap_transport_handle_t  transport_handle;   // CoAP transport handle (socket descriptor).

    // Notification values
    struct k_sem             response_received;  // Given in lwm2m_notification()
    uint8_t                  response_coap_code; // Response from server
    uint32_t                 response_err_code;  // Internal error code

    // Context related work items
    struct k_delayed_work    register_work;      // Register work item
    struct k_delayed_work    update_work;        // Update work item
    struct k_delayed_work    disable_work;       // Disable work item

    // State context values
    uint16_t                 security_instance;  // Security object instance
    uint16_t                 server_instance;    // Server object instance
    uint16_t                 short_server_id;
    uint8_t                  retry_count;        // Found in lwm2m_retry_delay.c
    uint8_t                  flags;              // CLIENT_FLAG settings
} client_context_t;

static struct k_sem             m_bootstrap_done;
static struct k_sem             m_connect_lock;
static struct k_sem             m_pdn_lock;
static struct k_delayed_work    bootstrap_work;

#define GET_AND_CHECK_CTX(field, value) \
    client_context_t *ctx = NULL; \
    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) \
        if (m_client_context[i].field == value) \
            ctx = &m_client_context[i]; \
    if (ctx == NULL) \
        return -EINVAL

#define LWM2M_MAX_CONNECTIONS 2                  // Maximum number of server connections.
K_THREAD_STACK_ARRAY_DEFINE(m_client_stack, LWM2M_MAX_CONNECTIONS, 1536);

#define LWM2M_CLIENT_SEC_TAG_OFFSET    25
#define LWM2M_LOCAL_CLIENT_PORT_OFFSET 9998      // Local port to connect to the LWM2M server.

#define LIFETIME_UPDATE_FACTOR .9

static client_context_t        m_client_context[LWM2M_MAX_CONNECTIONS];
extern lwm2m_client_identity_t m_client_id; // Todo: Fix this

static inline bool client_is_work_q_started(client_context_t *ctx)
{
    return (ctx->flags & CLIENT_FLAG_WORK_Q_STARTED);
}

static inline bool client_is_secure(client_context_t *ctx)
{
    return (ctx->flags & CLIENT_FLAG_SECURE_CONNECTION);
}

static inline bool client_is_registered(client_context_t *ctx)
{
    return (ctx->flags & CLIENT_FLAG_IS_REGISTERED);
}

static inline void client_set_work_q_started(client_context_t *ctx)
{
    ctx->flags |= CLIENT_FLAG_WORK_Q_STARTED;
}

static inline void client_set_secure(client_context_t *ctx)
{
    ctx->flags |= CLIENT_FLAG_SECURE_CONNECTION;
}

static inline bool client_is_configured(client_context_t *ctx)
{
    return (ctx->security_instance != UINT16_MAX);
}

static inline bool client_is_registration_done(void)
{
    bool is_registered = true;

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        if (client_is_configured(&m_client_context[i]) &&
            !(m_client_context[i].flags & CLIENT_FLAG_IS_REGISTERED)) {
            is_registered = false;
        }
    }

    return is_registered;
}

static inline void client_set_registered(client_context_t *ctx, bool registered)
{
    if (operator_is_vzw(true) &&
        (lwm2m_server_registered_get(ctx->server_instance) != registered)) {
        // Server registered is VzW only.
        lwm2m_server_registered_set(ctx->server_instance, registered);
        lwm2m_storage_server_store();
    }

    if (registered) {
        ctx->flags |= CLIENT_FLAG_IS_REGISTERED;

        if (client_is_registration_done()) {
            // Set to bootstrapped in case this has not been set before.
            lwm2m_set_bootstrapped(true);

            lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_REGISTERED, NULL);
        }
    } else {
        ctx->flags &= ~CLIENT_FLAG_IS_REGISTERED;
    }
}

static void client_cancel_all_tasks(client_context_t *ctx)
{
    k_delayed_work_cancel(&bootstrap_work);
    k_delayed_work_cancel(&ctx->register_work);
    k_delayed_work_cancel(&ctx->update_work);
    k_delayed_work_cancel(&ctx->disable_work);
}

static int client_event_deferred(uint32_t reason, int32_t timeout)
{
    lwm2m_carrier_event_deferred_t deferred_event = {
        .reason  = reason,
        .timeout = timeout
    };

    return lwm2m_main_event_notify(LWM2M_CARRIER_EVENT_DEFERRED, &deferred_event);
}

static char *client_apn(client_context_t *ctx)
{
    if (ctx->flags & CLIENT_FLAG_CONNECTION_USE_APN) {
        return lwm2m_pdn_current_apn();
    }

    return lwm2m_pdn_default_apn();
}

static bool client_use_pdn_connection(client_context_t *ctx)
{
    bool use_pdn_connection = false;

    if (operator_is_vzw(false)) {
        // VzW: Setup PDN for all servers except Repository
        if (ctx->short_server_id != 1000) {
            use_pdn_connection = true;
        }
    } else if (operator_is_att(false)) {
        // AT&T: Setup PDN unless using default (CID 0)
        uint16_t default_apn_instance = lwm2m_apn_conn_prof_default_instance();
        if (lwm2m_apn_instance() != default_apn_instance) {
            use_pdn_connection = true;
        }
    }

    return use_pdn_connection;
}

static int client_pdn_setup(client_context_t *ctx, bool *pdn_activated)
{
    nrf_sa_family_t pdn_type_allowed = 0;

    if (client_use_pdn_connection(ctx)) {
        k_sem_take(&m_pdn_lock, K_FOREVER);
        bool activated = lwm2m_pdn_activate(pdn_activated, &pdn_type_allowed);
        k_sem_give(&m_pdn_lock);

        if (!activated) {
            return NRF_ENETDOWN;
        }

        ctx->flags |= CLIENT_FLAG_CONNECTION_USE_APN;
    } else {
        pdn_type_allowed = lwm2m_pdn_type_allowed();
        ctx->flags &= ~CLIENT_FLAG_CONNECTION_USE_APN;
    }

    if (ctx->family_type == 0) {
        // Set family type only if not already set for this connection.
        if (pdn_type_allowed != 0) {
            // PDN type restrictions. Use only this.
            ctx->family_type = pdn_type_allowed;
            ctx->flags &= ~CLIENT_FLAG_IP_FALLBACK_POSSIBLE;
        } else if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_IPv6)) {
            // IPv6 disabled.
            ctx->family_type = NRF_AF_INET;
            ctx->flags &= ~CLIENT_FLAG_IP_FALLBACK_POSSIBLE;
        } else if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_FALLBACK)) {
            // Fallback disabled.
            ctx->family_type = NRF_AF_INET6;
            ctx->flags &= ~CLIENT_FLAG_IP_FALLBACK_POSSIBLE;
        } else {
            // No PDN type restrictions. Start with IPv6.
            ctx->family_type = NRF_AF_INET6;
            ctx->flags |= CLIENT_FLAG_IP_FALLBACK_POSSIBLE;
        }
    }

    return 0;
}

static int32_t client_select_next_apn(client_context_t *ctx)
{
    int32_t delay = 0;

    k_sem_take(&m_pdn_lock, K_FOREVER);

    // Deactivate in case the PDN socket is still open.
    lwm2m_pdn_deactivate();

    // Supported family type may be different for next APN.
    ctx->family_type = 0;
    ctx->flags &= ~CLIENT_FLAG_IP_FALLBACK_POSSIBLE;

    if (lwm2m_pdn_next_enabled_apn_instance()) {
        // Moved back to first APN, use retry back off period.
        delay = SECONDS(lwm2m_conn_ext_apn_retry_back_off_period_get(0, lwm2m_apn_instance()));
    }

    k_sem_give(&m_pdn_lock);

    return delay;
}

static int client_dns_request(client_context_t *ctx)
{
    LWM2M_INF("* DNS request using %s (APN %s) [%u]",
              (ctx->family_type == NRF_AF_INET6) ? "IPv6" : "IPv4",
              lwm2m_os_log_strdup(client_apn(ctx)), ctx->short_server_id);

    char uri_copy[128];
    char *p_server_uri;
    uint8_t uri_len = 0;
    const char *p_hostname;
    uint16_t port;
    bool secure;

    p_server_uri = lwm2m_security_server_uri_get(ctx->security_instance, &uri_len);

    if (uri_len >= sizeof(uri_copy)) {
        return NRF_EINVAL;
    }

    // Copy to make a 0-terminated string.
    strncpy(uri_copy, p_server_uri, uri_len);
    uri_copy[uri_len] = '\0';

    p_hostname = client_parse_uri(uri_copy, uri_len, &port, &secure);
    if (secure) {
        client_set_secure(ctx);
    }

    if (p_hostname == NULL) {
        return NRF_EINVAL;
    }

    struct nrf_addrinfo hints = {
        .ai_family = ctx->family_type,
        .ai_socktype = NRF_SOCK_DGRAM
    };

    // Structures that might be pointed to by APN hints.
    struct nrf_addrinfo apn_hints;

    if (ctx->flags & CLIENT_FLAG_CONNECTION_USE_APN) {
        apn_hints.ai_family    = NRF_AF_LTE;
        apn_hints.ai_socktype  = NRF_SOCK_MGMT;
        apn_hints.ai_protocol  = NRF_PROTO_PDN;
        apn_hints.ai_canonname = lwm2m_pdn_current_apn();

        hints.ai_next = &apn_hints;
    }

    struct nrf_addrinfo *result;
    int ret_val = -1;
    int cnt = 1;

    // TODO:
    //  getaddrinfo() currently returns a mix of GAI error codes and NRF error codes.
    //  22 = NRF_EINVAL is invalid argument, but may also indicate no address found in the DNS query response.
    //  60 = NRF_ETIMEDOUT is a timeout waiting for DNS query response.
    //  50 = NRF_ENETDOWN is PDN down.
    while (ret_val != 0 && cnt <= 5) {
        ret_val = nrf_getaddrinfo(p_hostname, NULL, &hints, &result);
        if (ret_val != 0) {
            if (ret_val == NRF_EINVAL || ret_val == NRF_ETIMEDOUT || ret_val == NRF_ENETDOWN) {
                break;
            }
            lwm2m_os_sleep(1000 * cnt);
        }
        cnt++;
    }

    if (ret_val == NRF_EINVAL || ret_val == NRF_ETIMEDOUT) {
        LWM2M_WRN("* No %s address found for \"%s\"", (ctx->family_type == NRF_AF_INET6) ? "IPv6" : "IPv4", lwm2m_os_log_strdup(p_hostname));
        return NRF_ENETUNREACH;
    } else if (ret_val == NRF_ENETDOWN) {
        LWM2M_ERR("* Failed to lookup \"%s\": PDN down", lwm2m_os_log_strdup(p_hostname));
        return NRF_EAGAIN; // Return EAGAIN so we come back setup PDN again
    } else if (ret_val != 0) {
        LWM2M_ERR("* Failed to lookup \"%s\": %d", lwm2m_os_log_strdup(p_hostname), ret_val);
        return ret_val;
    }

    client_init_sockaddr_in(&ctx->remote_server, result->ai_addr, result->ai_family, port);
    nrf_freeaddrinfo(result);

    if (IS_ENABLED(CONFIG_NRF_LWM2M_ENABLE_LOGS)) {
        const char *p_ip_address = client_remote_ntop(&ctx->remote_server);
        LWM2M_INF("* DNS result: %s [%u]", lwm2m_os_log_strdup(p_ip_address), ctx->short_server_id);
    }

    return 0;
}

static void client_update_server_conf(client_context_t *ctx)
{
    ctx->server_conf.lifetime = lwm2m_server_lifetime_get(ctx->server_instance);

    if (operator_is_att(false) &&
        !lwm2m_security_is_bootstrap_server_get(ctx->security_instance)) {
        // For AT&T MSISDN is fetched from the connectivity extension object.
        uint8_t msisdn_len = 0;
        ctx->server_conf.msisdn.p_val = lwm2m_conn_ext_msisdn_get(&msisdn_len);
        ctx->server_conf.msisdn.len = msisdn_len;
    }
}

static void client_init_server_conf(client_context_t *ctx)
{
    // Initialize server configuration structure.
    memset(&ctx->server_conf, 0, sizeof(ctx->server_conf));

    // Set the short server id of the server in the config.
    ctx->server_conf.short_server_id = ctx->short_server_id;

    if (operator_is_supported(false)) {
        ctx->server_conf.binding.p_val = "UQS";
        ctx->server_conf.binding.len = 3;

        if (!operator_is_att(false) &&
            !lwm2m_security_is_bootstrap_server_get(ctx->security_instance)) {
            ctx->server_conf.msisdn.p_val = lwm2m_msisdn_get();
            ctx->server_conf.msisdn.len = strlen(lwm2m_msisdn_get());
        }
    }

    client_update_server_conf(ctx);
}

static int client_session_setup(client_context_t *ctx)
{
    LWM2M_INF("* Setup %ssecure session (APN %s) [%u]",
              (client_is_secure(ctx) ? "" : "non-"),
              lwm2m_os_log_strdup(client_apn(ctx)), ctx->short_server_id);

    struct nrf_sockaddr_in6 local_addr;
    client_init_sockaddr_in(&local_addr, NULL, ctx->remote_server.sin6_family,
                            LWM2M_LOCAL_CLIENT_PORT_OFFSET + ctx->security_instance);

    nrf_sec_tag_t sec_tag_list[1] = { LWM2M_CLIENT_SEC_TAG_OFFSET + ctx->security_instance };

    coap_sec_config_t setting =
    {
        .role          = 0,    // 0 -> Client role
        .session_cache = 0,    // 1 -> Enable session cache
        .sec_tag_count = 1,    // One sec_tag in use.
        .sec_tag_list  = sec_tag_list
    };

    coap_local_t local_port =
    {
        .addr         = (struct nrf_sockaddr *)&local_addr,
        .setting      = &setting,
        .protocol     = (client_is_secure(ctx) ? NRF_SPROTO_DTLS1v2 : NRF_IPPROTO_UDP)
    };

    if (ctx->flags & CLIENT_FLAG_CONNECTION_USE_APN) {
        local_port.interface = lwm2m_pdn_current_apn();
    }

    // Modem can only handle one DTLS handshake
    k_sem_take(&m_connect_lock, K_FOREVER);
    ctx->flags |= CLIENT_FLAG_IS_CONNECTING;
    uint32_t err_code = coap_security_setup(&local_port, (struct nrf_sockaddr *)&ctx->remote_server);
    ctx->flags &= ~CLIENT_FLAG_IS_CONNECTING;
    k_sem_give(&m_connect_lock);

    if (err_code == 0) {
        if (client_is_secure(ctx)) {
            LWM2M_INF("* Connected [%u]", ctx->short_server_id);
        }

        ctx->transport_handle = local_port.transport;
    } else if (err_code == EIO && (lwm2m_os_errno() == NRF_ENETDOWN)) {
        LWM2M_INF("* Connection failed (PDN down) [%u]", ctx->short_server_id);
        err_code = NRF_EAGAIN; // Return EAGAIN so we come back setup PDN again
    } else {
        LWM2M_INF("* Connection failed: %s (%ld), %s (%d) [%u]",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  ctx->short_server_id);
        err_code = lwm2m_os_errno();
    }

    return err_code;
}

static void client_disconnect(client_context_t *ctx)
{
    if (ctx->transport_handle != -1) {
        coap_security_destroy(ctx->transport_handle);
        ctx->transport_handle = -1;
    }
}

/**
 * @brief Schedule connect failure retry.
 *
 * Todo: Add detailed documentation how retry is handled.
 * - PDN detected down while doing DNS or connect(), EAGAIN to retry immediately
 * - Error activating PDN
 *   - VzW: Use connect retry timeouts?
 *   - AT&T: APN fallback on last retry
 * - APN fallback because of no response from server (AT&T)
 * - Fallback to the other IP version (if both versions supported)
 * - Todo: Figure out retry delay for AT&T. Same as PDN retry? Recreate PDN?
 */
static void client_schedule_retry(client_context_t *ctx, struct k_delayed_work *work, int reason)
{
    bool retry_handled = false;
    int32_t delay = 0;
    bool is_last = false;

    if (reason == NRF_EAGAIN) {
        // Retry without delay
        retry_handled = true;
    }

    if (reason == NRF_ENETDOWN) {
        // Retry delay because of error activating PDN.
        delay = lwm2m_retry_delay_pdn_get(lwm2m_apn_instance(), &is_last);

        if (is_last && operator_is_att(true)) {
            // Last PDN retry has failed, try next APN.
            LWM2M_INF("Next APN fallback (activate failure)");
            delay = client_select_next_apn(ctx);
        }
        retry_handled = true;
    }

    if ((reason == NRF_ENETUNREACH) && operator_is_att(true) &&
        (!(ctx->flags & CLIENT_FLAG_IP_FALLBACK_POSSIBLE) || (ctx->family_type == NRF_AF_INET))) {
        // APN fallback because of no response from server (IPv6 and/or IPv4).
        LWM2M_INF("Next APN fallback (network unreachable)");
        delay = client_select_next_apn(ctx);
        retry_handled = true;
    }

    // Done handling PDN.
    if (retry_handled && delay) {
        LWM2M_INF("PDN retry delay for %ld seconds [%u]", delay / SECONDS(1), ctx->short_server_id);
        client_event_deferred(LWM2M_CARRIER_DEFERRED_PDN_ACTIVATE, delay);
    }

    if ((reason == NRF_ENETUNREACH) && (ctx->flags & CLIENT_FLAG_IP_FALLBACK_POSSIBLE)) {
        // Fallback to the other IP version.
        ctx->family_type = (ctx->family_type == NRF_AF_INET6) ? NRF_AF_INET : NRF_AF_INET6;
        if (ctx->family_type == NRF_AF_INET) {
            LWM2M_INF("IPv6 to IPv4 fallback");
            retry_handled = true;
        }
    }

    if (!retry_handled) {
        // Connection retry delay.
        delay = lwm2m_retry_delay_connect_next(ctx->security_instance, &is_last);

        if (delay == -1) {
            LWM2M_ERR("Bootstrap procedure failed");
            lwm2m_retry_delay_connect_reset(ctx->security_instance);
            lwm2m_main_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, 0);
        }

        if (is_last && (reason == NRF_ENETUNREACH)) {
            // This is the last retry delay after no response from server.
            // Disconnect the session and retry on timeout.
            client_disconnect(ctx);
        }

        if (delay) {
            // Todo: Add event deferred
            // uint32_t reason = app_event_deferred_reason(fallback);
            // app_event_deferred(reason, retry_delay / SECONDS(1));
            LWM2M_INF("Connect retry delay for %ld minutes [%u]", delay / MINUTES(1), ctx->short_server_id);
        }
    }

    if (delay >= 0) {
        k_delayed_work_submit_to_queue(&ctx->work_q, work, K_MSEC(delay));
    }
}

static void client_schedule_update(client_context_t *ctx)
{
    int32_t delay = lwm2m_server_lifetime_get(ctx->server_instance) * LIFETIME_UPDATE_FACTOR;
    k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_SECONDS(delay));
}

static uint32_t client_connect(client_context_t *ctx, bool *did_connect)
{
    bool pdn_activated = false;
    uint32_t err_code;

    err_code = client_pdn_setup(ctx, &pdn_activated);

    if (err_code != 0 || pdn_activated) {
        // When PDN is activated we most likely got a new IP
        client_disconnect(ctx);
    }

    if (ctx->transport_handle != -1) {
        // Already connected
        return err_code;
    }

    if (did_connect) {
        *did_connect = true;
    }

    if (err_code == 0) {
        err_code = client_dns_request(ctx);
    }

    if (err_code == 0) {
        err_code = client_session_setup(ctx);
    }

    return err_code;
}

static void client_configure(client_context_t *ctx,
                             uint16_t          security_instance,
                             uint16_t          short_server_id)
{
    ctx->security_instance = security_instance;
    ctx->server_instance = UINT16_MAX;
    ctx->short_server_id = short_server_id;
    ctx->family_type = 0; // Will be set in client_pdn_setup() when connecting

    // Todo: Initialize all other context values

    // Find the server instance matching the security instance
    for (int server_instance = 0; server_instance < 1+LWM2M_MAX_SERVERS; server_instance++) {
        if (short_server_id == lwm2m_server_short_server_id_get(server_instance)) {
            ctx->server_instance = server_instance;
            if (IS_ENABLED(CONFIG_NRF_LWM2M_ENABLE_LOGS)) {
                uint16_t access_control;
                if (lwm2m_access_control_find(LWM2M_OBJ_SERVER, server_instance, &access_control) == 0) {
                    LWM2M_INF("| </0/%u>,</1/%u>,</2/%u>;ssid=%u",
                            security_instance, server_instance, access_control, short_server_id);
                } else {
                    LWM2M_INF("| </0/%u>,</1/%u>;ssid=%u",
                            security_instance, server_instance, short_server_id);
                }
            }
            break;
        }
    }

    if (IS_ENABLED(CONFIG_NRF_LWM2M_ENABLE_LOGS)) {
        if (ctx->server_instance == UINT16_MAX) {
            LWM2M_INF("| </0/%u>;ssid=%u", security_instance, short_server_id);
        }
    }
}

static int client_update_observers(client_context_t *ctx)
{
    coap_observer_t *p_observer = NULL;

    // Update all observers after a reconnect.
    while (coap_observe_server_next_get(&p_observer, p_observer, NULL) == 0) {
        // Todo: remote address may have changed
        if (memcmp(p_observer->remote, &ctx->remote_server, sizeof(struct nrf_sockaddr)) == 0) {
            p_observer->transport = ctx->transport_handle;
        }
    }

    return 0;
}

static int client_remove_observers(client_context_t *ctx)
{
    coap_observer_t *p_observer = NULL;

    // Remove all observers after deregister.
    while (coap_observe_server_next_get(&p_observer, p_observer, NULL) == 0) {
        // Todo: remote address may have changed
        if (memcmp(p_observer->remote, &ctx->remote_server, sizeof(struct nrf_sockaddr)) == 0) {
            lwm2m_observe_unregister(p_observer->remote, p_observer->resource_of_interest);
        }
    }

    return 0;
}

static void client_bootstrap_complete(void)
{
    lwm2m_security_bootstrapped_set(true);
    lwm2m_client_configure();

    // Client hold off timer is only used after bootstrap.
    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        m_client_context[i].flags |= CLIENT_FLAG_USE_HOLDOFF_TIMER;
    }

    // Main state will trigger lwm2m_client_connect() when
    // having link after writing new credentials.
    lwm2m_main_bootstrap_done();
}

static bool client_register_deferred(client_context_t *ctx)
{
    if (operator_is_vzw(true) &&
        (ctx->flags & CLIENT_FLAG_USE_HOLDOFF_TIMER) &&
        (ctx->short_server_id == 1000)) {
        // VzW ssid 1000 is deferred to be registered after ssid 102
        // when using holdoff timer.
        return true;
    }

    return false;
}

static int client_register_done(client_context_t *ctx)
{
    if (operator_is_vzw(true) && (ctx->short_server_id == 102)) {
        ctx = &m_client_context[1]; // Todo: Loop to find ssid 1000
        if ((ctx->short_server_id == 1000) &&
            (ctx->flags & CLIENT_FLAG_USE_HOLDOFF_TIMER)) {
            int32_t delay = lwm2m_server_client_hold_off_timer_get(ctx->server_instance);
            ctx->flags &= ~CLIENT_FLAG_USE_HOLDOFF_TIMER;
            LWM2M_INF(": Register (%ds) [%u]", delay, ctx->short_server_id);
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_SECONDS(delay));
        }
    }

    return 0;
}

static int client_update_done(client_context_t *ctx)
{
    // Update observers after doing a reconnect.
    if (lwm2m_remote_reconnecting_get(ctx->short_server_id)) {
        client_update_observers(ctx);
        lwm2m_remote_reconnecting_clear(ctx->short_server_id);
        lwm2m_observer_process(true);
    }

    return 0;
}

static uint32_t client_gen_link_format(uint16_t  short_server_id,
                                       uint8_t **pp_link_format,
                                       uint16_t *p_link_format_len)
{
    uint32_t err_code;

    // Dry run the link format generation, to check how much memory that is needed.
    err_code = lwm2m_coap_handler_gen_link_format(LWM2M_INVALID_INSTANCE, short_server_id,
                                                  NULL, p_link_format_len);

    if (err_code == 0) {
        // Allocate the needed amount of memory.
        *pp_link_format = lwm2m_os_malloc(*p_link_format_len);

        if (*pp_link_format == NULL) {
            err_code = NRF_ENOMEM;
        }
    }

    if (err_code == 0) {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(LWM2M_INVALID_INSTANCE, short_server_id,
                                                      *pp_link_format, p_link_format_len);
    }

    return err_code;
}

static void client_free(client_context_t *ctx)
{
    client_cancel_all_tasks(ctx);
    lwm2m_remote_deregister(ctx->short_server_id);

    // Todo: Initialize all context values needed

    ctx->family_type = 0;
    memset(&ctx->server_conf, 0, sizeof(ctx->server_conf));
    memset(&ctx->remote_server, 0, sizeof(ctx->remote_server));
    client_disconnect(ctx);

    ctx->security_instance = UINT16_MAX;
    ctx->server_instance = UINT16_MAX;
    ctx->short_server_id = 0;
    ctx->retry_count = 0;
    ctx->flags &= CLIENT_FLAG_WORK_Q_STARTED; // Keep WORK_Q_STARTED
}

static void client_bootstrap_task(struct k_work *work)
{
    // Bootstrap task is always context 0.
    client_context_t *ctx = &m_client_context[0];
    uint32_t err_code;

    LWM2M_INF("Client bootstrap [%u]", ctx->short_server_id);

    err_code = client_connect(ctx, NULL);

    if (err_code != 0) {
        client_schedule_retry(ctx, &bootstrap_work, err_code);
        return;
    }

    // Always register the remote server address when doing connect because it may have changed.
    err_code = lwm2m_remote_register(ctx->short_server_id, (struct nrf_sockaddr *)&ctx->remote_server);

    lwm2m_main_bootstrap_reset();

    err_code = lwm2m_bootstrap((struct nrf_sockaddr *)&ctx->remote_server,
                               &m_client_id, ctx->transport_handle, NULL);

    if (err_code == 0) {
        ctx->retry_count = 0;

        // Wait for CoAP response.
        k_sem_take(&ctx->response_received, K_FOREVER);

        // Valid response codes for Bootstrap-Request
        //   2.04 Changed - Bootstrap-Request is completed successfully
        //   4.00 Bad Request - Unknown Endpoint Client Name

        if (ctx->response_coap_code == COAP_CODE_204_CHANGED) {
            // Wait for bootstrap transfer to complete.
            if (k_sem_take(&m_bootstrap_done, K_SECONDS(20)) == 0) {
                LWM2M_INF("Bootstrap done");
                lwm2m_retry_delay_connect_reset(ctx->security_instance);
                client_disconnect(ctx);
                client_bootstrap_complete();
            } else {
                LWM2M_INF("Bootstrap timed out");
                client_disconnect(ctx);
                client_schedule_retry(ctx, &bootstrap_work, NRF_ETIMEDOUT);
            }
        } else if ((ctx->response_coap_code == 0) ||
                   (ctx->response_coap_code == COAP_CODE_403_FORBIDDEN)) {  // VzW may report this
            // No response or received a 4.03 error.
            client_schedule_retry(ctx, &bootstrap_work, NRF_ETIMEDOUT);
        } else {
            // 4.00 Bad Request or not a valid response code.
            LWM2M_ERR("Bootstrap procedure failed (%d.%02d)",
                      ctx->response_coap_code >> 5, ctx->response_coap_code & 0x1f);
            client_disconnect(ctx);
            lwm2m_retry_delay_connect_reset(ctx->security_instance);
            lwm2m_main_event_error(LWM2M_CARRIER_ERROR_BOOTSTRAP, 0);
        }
    } else if (lwm2m_os_errno() != NRF_EAGAIN || ctx->retry_count >= 5) {
        ctx->retry_count = 0;
        LWM2M_INF("Bootstrap failed: %s (%d), %s (%d), reconnect [%u]",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  ctx->short_server_id);

        client_disconnect(ctx);
        client_schedule_retry(ctx, &bootstrap_work, lwm2m_os_errno());
    } else {
        ctx->retry_count++;
        LWM2M_WRN("Bootstrap retry (#%u) [%u]", ctx->retry_count, ctx->short_server_id);
        k_delayed_work_submit_to_queue(&ctx->work_q, &bootstrap_work, K_MSEC(100));
    }
}

static void client_register_task(struct k_work *work)
{
    client_context_t *ctx = CONTAINER_OF(work, client_context_t, register_work);
    bool did_connect = false;
    uint32_t err_code;

    LWM2M_INF("Client register [%u]", ctx->short_server_id);

    err_code = client_connect(ctx, &did_connect);

    if (err_code != 0) {
        client_schedule_retry(ctx, &ctx->register_work, err_code);
        return;
    }

    client_init_server_conf(ctx);

    uint16_t link_format_len = 0;
    uint8_t *p_link_format = NULL;
    err_code = client_gen_link_format(ctx->short_server_id, &p_link_format, &link_format_len);

    if (err_code == 0) {
        err_code = lwm2m_register((struct nrf_sockaddr *)&ctx->remote_server,
                                  &m_client_id, &ctx->server_conf, ctx->transport_handle,
                                  p_link_format, link_format_len);
    }

    if (p_link_format) {
        lwm2m_os_free(p_link_format);
    }

    if (err_code == 0) {
        ctx->retry_count = 0;

        // Wait for CoAP response.
        k_sem_take(&ctx->response_received, K_FOREVER);

        // Valid response codes for Register
        //   2.01 Created - “Register” operation is completed successfully
        //   4.00 Bad Request - The mandatory parameter is not specified or unknown parameter is specified
        //   4.03 Forbidden - The Endpoint Client Name registration in the LwM2M Server is not allowed
        //   4.12 Precondition Failed - Supported LwM2M Versions of the Server and the Client are not compatible

        if ((ctx->response_coap_code == COAP_CODE_201_CREATED) ||
            (ctx->response_coap_code == COAP_CODE_204_CHANGED)) {  // VzW may report this
            // We have successfully registered, schedule update.
            LWM2M_INF("Registered [%u]", ctx->short_server_id);
            lwm2m_retry_delay_connect_reset(ctx->security_instance);
            lwm2m_storage_location_store();
            client_set_registered(ctx, true);
            lwm2m_notif_attr_storage_restore(ctx->short_server_id);

            client_register_done(ctx);
            client_schedule_update(ctx);
        } else if (operator_is_vzw(true) &&
                   (ctx->short_server_id == 102) &&
                   (ctx->response_coap_code == COAP_CODE_400_BAD_REQUEST)) {
            // Received 4.00 error from VzW DM server, retry in 24 hours.
            // Todo: reset retry delay
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_HOURS(24));
        } else if (ctx->response_coap_code == 0 && !did_connect) {
            // No response from register request, try again.
            LWM2M_INF("Register timeout, reconnect [%u]", ctx->short_server_id);
            lwm2m_remote_deregister(ctx->short_server_id);
            client_disconnect(ctx);
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_NO_WAIT);
        } else {
            // Received a unknown response code or timeout immediately after connect.
            client_schedule_retry(ctx, &ctx->register_work, NRF_ETIMEDOUT);
        }
    } else if (lwm2m_os_errno() != NRF_EAGAIN || ctx->retry_count >= 5) {
        ctx->retry_count = 0;
        LWM2M_INF("Register failed: %s (%d), %s (%d), reconnect [%u]",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  ctx->short_server_id);

        client_disconnect(ctx);
        client_schedule_retry(ctx, &ctx->register_work, lwm2m_os_errno());
    } else {
        ctx->retry_count++;
        LWM2M_WRN("Register retry (#%u) [%u]", ctx->retry_count, ctx->short_server_id);
        k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_MSEC(100));
    }
}

static void client_update_task(struct k_work *work)
{
    client_context_t *ctx = CONTAINER_OF(work, client_context_t, update_work);
    bool did_connect = false;
    uint32_t err_code;

    LWM2M_INF("Client update [%u]", ctx->short_server_id);

    err_code = client_connect(ctx, &did_connect);

    if (err_code != 0) {
        client_schedule_retry(ctx, &ctx->update_work, err_code);
        return;
    }

    if (client_is_registered(ctx)) {
        // Lifetime or MSISDN may have changed.
        client_update_server_conf(ctx);
    } else {
        client_init_server_conf(ctx);
    }

    if (did_connect) {
        // Always register the remote server address when doing connect because it may have changed.
        err_code = lwm2m_remote_register(ctx->short_server_id, (struct nrf_sockaddr *)&ctx->remote_server);
    }

    // Todo: Sync "connect_update" with app_server_update()

    err_code = lwm2m_update((struct nrf_sockaddr *)&ctx->remote_server,
                            &ctx->server_conf, ctx->transport_handle);

    if (err_code == 0) {
        ctx->retry_count = 0;

        // Wait for CoAP response.
        k_sem_take(&ctx->response_received, K_FOREVER);

        // Valid response codes for Update
        //   2.04 Changed - “Update” operation is completed successfully
        //   4.00 Bad Request - The mandatory parameter is not specified or unknown parameter is specified
        //   4.04 Not Found - URI of “Update” operation is not found

        if (ctx->response_coap_code == COAP_CODE_204_CHANGED) {
            // We have successfully updated, schedule next update.
            LWM2M_INF("Updated [%u]", ctx->short_server_id);
            lwm2m_retry_delay_connect_reset(ctx->security_instance);
            if (!client_is_registered(ctx)) {
                lwm2m_observer_storage_restore(ctx->short_server_id, ctx->transport_handle);
                lwm2m_notif_attr_storage_restore(ctx->short_server_id);
                client_set_registered(ctx, true);
            }

            client_update_done(ctx);
            client_schedule_update(ctx);
        } else if ((ctx->response_coap_code == COAP_CODE_400_BAD_REQUEST) ||
                   (ctx->response_coap_code == COAP_CODE_403_FORBIDDEN)   ||  // AT&T reports this when different DTLS session
                   (ctx->response_coap_code == COAP_CODE_404_NOT_FOUND)) {
            // Remove the server (deregister) to trigger a Registration
            // instead of an Update the next time we connect to it.
            lwm2m_remote_deregister(ctx->short_server_id);
            lwm2m_storage_location_store();
            client_set_registered(ctx, false);

            // Go back to Register
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_NO_WAIT);
        } else if (ctx->response_coap_code == 0 && !did_connect) {
            // No response from update request, try again.
            LWM2M_INF("Update timeout, reconnect [%u]", ctx->short_server_id);
            client_disconnect(ctx);
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_NO_WAIT);
        } else {
            // Received a unknown response code or timeout immediately after connect.
            client_schedule_update(ctx);
        }
    } else if (lwm2m_os_errno() != NRF_EAGAIN || ctx->retry_count >= 5) {
        ctx->retry_count = 0;
        LWM2M_INF("Update failed: %s (%d), %s (%d), reconnect [%u]",
                lwm2m_os_log_strdup(strerror(err_code)), err_code,
                lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                ctx->short_server_id);

        client_disconnect(ctx);
        if (did_connect) {
            client_schedule_retry(ctx, &ctx->update_work, lwm2m_os_errno());
        } else {
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_NO_WAIT);
        }
    } else {
        ctx->retry_count++;
        LWM2M_WRN("Update retry (#%u) [%u]", ctx->retry_count, ctx->short_server_id);
        k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_MSEC(100));
    }
}

static void client_disable_task(struct k_work *work)
{
    client_context_t *ctx = CONTAINER_OF(work, client_context_t, disable_work);
    bool did_connect = false;
    uint32_t err_code;

    LWM2M_INF("Client disable [%u]", ctx->short_server_id);

    k_delayed_work_cancel(&ctx->update_work);

    err_code = client_connect(ctx, &did_connect);

    if (err_code != 0) {
        client_schedule_retry(ctx, &ctx->disable_work, NRF_EAGAIN);
        return;
    }

    if (did_connect) {
        // Always register the remote server address when doing connect because it may have changed.
        err_code = lwm2m_remote_register(ctx->short_server_id, (struct nrf_sockaddr *)&ctx->remote_server);
    }

    err_code = lwm2m_deregister((struct nrf_sockaddr *)&ctx->remote_server,
                                ctx->transport_handle);

    client_remove_observers(ctx);

    if (err_code == 0) {
        ctx->retry_count = 0;

        // Wait for CoAP response.
        k_sem_take(&ctx->response_received, K_FOREVER);

        // Valid response codes for De-register
        //   2.02 Deleted - “De-register” operation is completed successfully
        //   4.00 Bad Request - Undetermined error occurred
        //   4.04 Not Found - URI of “De-register” operation is not found

        if (ctx->response_coap_code != 0 || did_connect) {
            // We have successfully deregistered, deregister failed or timeout immediately after connect.
            // In case of failure just continue because it's nothing more to do.
            lwm2m_storage_location_store();
            client_set_registered(ctx, false);

            int32_t delay = lwm2m_server_disable_timeout_get(ctx->server_instance);
            LWM2M_INF("Disable (%ld seconds) [%u]", delay, ctx->short_server_id);

            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->register_work, K_SECONDS(delay));
        } else {
            // No response from deregister request, try again.
            LWM2M_INF("Deregister timeout, reconnect [%u]", ctx->short_server_id);
            client_disconnect(ctx);
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->disable_work, K_NO_WAIT);
        }
    } else if (lwm2m_os_errno() != NRF_EAGAIN || ctx->retry_count >= 5) {
        ctx->retry_count = 0;
        LWM2M_ERR("Disable failed: %s (%d), %s (%d), reconnect [%u]",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  ctx->short_server_id);

        client_disconnect(ctx);
        if (did_connect) {
            client_schedule_retry(ctx, &ctx->disable_work, lwm2m_os_errno());
        } else {
            k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->disable_work, K_NO_WAIT);
        }
    } else {
        ctx->retry_count++;
        LWM2M_WRN("Disable retry (#%u) [%u]", ctx->retry_count, ctx->short_server_id);
        k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->disable_work, K_MSEC(100));
    }
}

int lwm2m_client_bootstrap_done(void)
{
    k_sem_give(&m_bootstrap_done);

    return 0;
}

void lwm2m_notification(lwm2m_notification_type_t   type,
                        struct nrf_sockaddr       * p_remote,
                        uint8_t                     coap_code,
                        uint32_t                    err_code)
{
    if (IS_ENABLED(CONFIG_NRF_LWM2M_ENABLE_LOGS)) {
        static char *str_type[] = { "Bootstrap", "Register", "Update", "Deregister" };
        LWM2M_INF("%s response %d.%02d (err:%lu)", str_type[type], coap_code >> 5, coap_code & 0x1f, err_code);
    }

    client_context_t *ctx = NULL;
    uint16_t short_server_id = 0;

    if (lwm2m_remote_short_server_id_find(&short_server_id, p_remote) != 0) {
        // Todo: Response received from unknown server. Handle this?
        LWM2M_WRN("Remote address not found");
        return;
    }

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        if (m_client_context[i].short_server_id == short_server_id) {
            ctx = &m_client_context[i];
        }
    }

    if (ctx) {
        ctx->response_coap_code = coap_code;
        ctx->response_err_code = err_code;

        k_sem_give(&ctx->response_received);
    }
}

static void client_init_work_q(int ctx_index)
{
    client_context_t *ctx = &m_client_context[ctx_index];

    client_set_work_q_started(ctx);

    k_work_q_start(&ctx->work_q, m_client_stack[ctx_index],
                    K_THREAD_STACK_SIZEOF(m_client_stack[ctx_index]),
                    K_LOWEST_APPLICATION_THREAD_PRIO);
    k_thread_name_set(&ctx->work_q.thread, "lwm2m_carrier_client");

    k_delayed_work_init(&ctx->register_work, client_register_task);
    k_delayed_work_init(&ctx->update_work, client_update_task);
    k_delayed_work_init(&ctx->disable_work, client_disable_task);

    k_sem_init(&ctx->response_received, 0, 1);
}

int lwm2m_client_init(void)
{
    static bool client_initialized;

    if (client_initialized) {
        // The client cannot be initialized more than once.
        return -EALREADY;
    }

    k_sem_init(&m_bootstrap_done, 0, 1);
    k_sem_init(&m_connect_lock, 1, 1);
    k_sem_init(&m_pdn_lock, 1, 1);
    k_delayed_work_init(&bootstrap_work, client_bootstrap_task);

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        m_client_context[i].transport_handle = -1;
    }

    client_initialized = true;

    return 0;
}

int lwm2m_client_configure(void)
{
    uint16_t bootstrap_instance = -1;

    LWM2M_INF("Client configure");

    // Free all client contexts
    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        client_free(&m_client_context[i]);
    }

    // Check if bootstrapped
    for (int security_instance = 0; security_instance < 1+LWM2M_MAX_SERVERS; security_instance++) {
        if (lwm2m_security_is_bootstrap_server_get(security_instance)) {
            bootstrap_instance = security_instance;
            break;
        }
    }

    bool is_bootstrapped = lwm2m_security_bootstrapped_get();
    uint8_t ctx_index = 0;

    // Fill client_context with one bootstrap server OR other servers
    for (int security_instance = 0; security_instance < 1+LWM2M_MAX_SERVERS; security_instance++) {
        uint16_t short_server_id = lwm2m_security_short_server_id_get(security_instance);

        uint8_t uri_len;
        char *p_uri = lwm2m_security_server_uri_get(security_instance, &uri_len);

        if ((short_server_id == 0) || // Instance is not initialized
            (uri_len == 0) || (p_uri == NULL) || // No URI for this instance
            ((security_instance == bootstrap_instance) && is_bootstrapped) ||
            ((security_instance != bootstrap_instance) && !is_bootstrapped)) {
            // Nothing to configure for this instance
            continue;
        }

        if (ctx_index >= ARRAY_SIZE(m_client_context)) {
            // Error, no room for this client instance
            continue;
        }

        if (!client_is_work_q_started(&m_client_context[ctx_index])) {
            client_init_work_q(ctx_index);
        }

        client_configure(&m_client_context[ctx_index], security_instance, short_server_id);
        ctx_index++;
    }

    return 0;
}

int lwm2m_client_connect(void)
{
    struct k_delayed_work *work;

    LWM2M_INF("Client connect trigger");
    // Todo: Give error if no clients are configured

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        client_context_t *ctx = &m_client_context[i];

        if (client_is_configured(ctx)) {
            // Start Bootstrap, Registration or Update work
            int32_t delay = 0;

            if (lwm2m_security_is_bootstrap_server_get(ctx->security_instance)) {
                delay = lwm2m_security_hold_off_timer_get();
                LWM2M_INF(": Bootstrap (%ds)", delay);
                work = &bootstrap_work;
            } else {
                if (lwm2m_remote_is_registered(ctx->short_server_id)) {
                    delay = 0;
                    LWM2M_INF(": Update [%u]", ctx->short_server_id);
                    work = &ctx->update_work;
                } else {
                    if (client_register_deferred(ctx)) {
                        continue;
                    }
                    if (ctx->flags & CLIENT_FLAG_USE_HOLDOFF_TIMER) {
                        delay = lwm2m_server_client_hold_off_timer_get(ctx->server_instance);
                        ctx->flags &= ~CLIENT_FLAG_USE_HOLDOFF_TIMER;
                    }
                    LWM2M_INF(": Register (%ds) [%u]", delay, ctx->short_server_id);
                    work = &ctx->register_work;
                }
            }

            k_delayed_work_submit_to_queue(&ctx->work_q, work, K_SECONDS(delay));
        }
    }

    return 0;
}

int lwm2m_client_update(uint16_t server_instance)
{
    GET_AND_CHECK_CTX(server_instance, server_instance);

    LWM2M_INF("Client update trigger");

    if (!client_is_registered(ctx)) {
        return -ENOENT;
    }

    k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_NO_WAIT);

    return 0;
}

int lwm2m_client_disable(uint16_t server_instance)
{
    GET_AND_CHECK_CTX(server_instance, server_instance);

    LWM2M_INF("Client disable trigger");

    if (!client_is_registered(ctx)) {
        return -ENOENT;
    }

    client_cancel_all_tasks(ctx);
    k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->disable_work, K_NO_WAIT);

    return 0;
}

int lwm2m_client_reconnect(uint16_t security_instance)
{
    GET_AND_CHECK_CTX(security_instance, security_instance);

    LWM2M_INF("Client reconnect trigger");

    if (!client_is_registered(ctx)) {
        return -ENOENT;
    }

    client_cancel_all_tasks(ctx);
    client_disconnect(ctx);
    lwm2m_remote_reconnecting_set(ctx->short_server_id);
    k_delayed_work_submit_to_queue(&ctx->work_q, &ctx->update_work, K_NO_WAIT);

    return 0;
}

int lwm2m_client_disconnect(void)
{
    LWM2M_INF("Client disconnect trigger");

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        client_context_t *ctx = &m_client_context[i];

        client_cancel_all_tasks(ctx);
        client_disconnect(ctx);
    }

    k_sem_take(&m_pdn_lock, K_FOREVER);
    lwm2m_pdn_deactivate();
    k_sem_give(&m_pdn_lock);

    return 0;
}

#if CONFIG_SHELL
static int cmd_client_status(const struct shell *shell, size_t argc, char **argv)
{
    int32_t delay;

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        client_context_t *ctx = &m_client_context[i];

        if (client_is_configured(ctx)) {
            shell_print(shell, "Client SSID %u", ctx->short_server_id);
            if (ctx->flags & CLIENT_FLAG_IS_CONNECTING) {
                shell_print(shell, "  Connecting...");
                continue;
            }
            delay = k_delayed_work_remaining_get(&bootstrap_work);
            if (delay > 0) {
                shell_print(shell, "  Bootstrap in %d seconds", delay / 1000);
            }
            delay = k_delayed_work_remaining_get(&ctx->register_work);
            if (delay > 0) {
                shell_print(shell, "  Register in %d seconds", delay / 1000);
            }
            delay = k_delayed_work_remaining_get(&ctx->update_work);
            if (delay > 0) {
                shell_print(shell, "  Update in %d seconds", delay / 1000);
            }
        // } else {
        //     shell_print(shell, "Client %d:", i);
        //     shell_print(shell, "  Not configured");
        }
    }

    return 0;
}

static int cmd_client_print(const struct shell *shell, size_t argc, char **argv)
{
    uint16_t access_control;
    char objects_str[40];
    char family_str[10];

    for (int i = 0; i < ARRAY_SIZE(m_client_context); i++) {
        client_context_t *ctx = &m_client_context[i];

        if (ctx->security_instance != UINT16_MAX) {
            if (ctx->server_instance != UINT16_MAX) {
                if (lwm2m_access_control_find(LWM2M_OBJ_SERVER, ctx->server_instance, &access_control) == 0) {
                    snprintf(objects_str, sizeof(objects_str), "</0/%u> </1/%u> </2/%u>",
                             ctx->security_instance, ctx->server_instance, access_control);
                } else {
                    snprintf(objects_str, sizeof(objects_str), "</0/%u> </1/%u>",
                             ctx->security_instance, ctx->server_instance);
                }
            } else {
                snprintf(objects_str, sizeof(objects_str), "</0/%u>", ctx->security_instance);
            }
        } else {
            snprintf(objects_str, sizeof(objects_str), "<none>");
        }

        if (ctx->family_type == NRF_AF_INET6) {
            strcpy(family_str, "IPv6");
        } else if (ctx->family_type == NRF_AF_INET) {
            strcpy(family_str, "IPv4");
        } else {
            sprintf(family_str, "%u", ctx->family_type);
        }

        shell_print(shell, "Client %u", i);
        shell_print(shell, "  Objects            %s", objects_str);
        shell_print(shell, "  Short server id    %u", ctx->short_server_id);
        shell_print(shell, "  Family type        %s", family_str);
        shell_print(shell, "  Remote server      %s", client_remote_ntop(&ctx->remote_server));
        shell_print(shell, "  Transport handle   %d", ctx->transport_handle);
        shell_print(shell, "  Retry counter      %u", ctx->retry_count);
        shell_print(shell, "  Flags:             0x%02x", ctx->flags);
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_client,
    SHELL_CMD(print, NULL, "Print client parameters", cmd_client_print),
    SHELL_CMD(status, NULL, "Client status", cmd_client_status),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(client, &sub_client, "LwM2M client", NULL);
#endif // CONFIG_SHELL