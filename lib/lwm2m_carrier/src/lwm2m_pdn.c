/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <string.h>
#include <nrf_socket.h>

#include <lwm2m.h>
#include <lwm2m_os.h>
#include <lwm2m_pdn.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_apn_conn_prof.h>
#include <lwm2m_conn_ext.h>
#include <lwm2m_retry_delay.h>
#include <operator_check.h>
#include <at_interface.h>

#define MAX_APN_LENGTH 64                        /**< Maximum APN length. */
static char m_default_apn[MAX_APN_LENGTH];       /**< Default APN. */
static char m_current_apn[MAX_APN_LENGTH];       /**< Current APN. */

static int      m_pdn_handle = DEFAULT_PDN_FD;   /**< PDN connection handle. */
static int      m_pdn_cid = -1;                  /**< PDN Context Identifier. */
static uint16_t m_apn_instance;                  /**< Current APN index. */

static int pdn_cid_get(int fd)
{
    int8_t cid = -1;

    if (fd >= 0) {
        nrf_socklen_t len = sizeof(cid);
        int err = nrf_getsockopt(fd, NRF_SOL_PDN, NRF_SO_PDN_CONTEXT_ID, &cid, &len);
        if (err) {
            LWM2M_ERR("Unable to get CID of socket %d, errno=%d",
                    fd, lwm2m_os_errno());
            return -1;
        }

        if (cid < 0  || cid >= MAX_NUM_OF_PDN_CONTEXTS) {
            LWM2M_ERR("Invalid CID received from socket %d!", fd);
            return -1;
        }
    } else if (DEFAULT_PDN_FD == fd) {
        // Socket fd == -1 is handled as a default PDN (CID = 0)
        cid = 0;
    }
    return cid;
}

static int pdn_activate(int *fd, const char *apn, int *esm_code)
{
    int err;
    nrf_pdn_state_t active = 0;
    nrf_socklen_t len = sizeof(active);

    if (fd == NULL || apn == NULL) {
        LWM2M_ERR("PDN activation - invalid params: fd (%d) apn (%d)", fd, (int)apn);
        return -1;
    }

    if (*fd != DEFAULT_PDN_FD) {
        /* If handle is valid, check if PDN is active */
        err = nrf_getsockopt(*fd, NRF_SOL_PDN, NRF_SO_PDN_STATE, &active, &len);
        if (err) {
            LWM2M_ERR("Reading PDN state failed: %s (%d)",
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());
        } else if (active) {
            return 0;
        }

        /* PDN is not active, close socket and reactivate it */
        nrf_close(*fd);
    }

    *fd = nrf_socket(NRF_AF_LTE, NRF_SOCK_MGMT, NRF_PROTO_PDN);
    if (*fd < 0) {
        LWM2M_ERR("PDN socket failed: %s (%d)",
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());
        return -1;
    }

    const int cid = pdn_cid_get(*fd);

    /* Reset ESM error code after successful PDN creation */
    at_esm_error_code_reset(cid);

    /* Connect to the PDN. */
    err = nrf_connect(*fd, (struct nrf_sockaddr *)apn, strlen(apn));
    if (err) {
        LWM2M_ERR("PDN connect failed: %s (%d), ESM error code: %d",
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno(),
                  at_esm_error_code_get(cid));

        int timeout_ms = 100; // 100 millisecond timeout.
        while (at_esm_error_code_get(cid) == 0 && timeout_ms > 0) {
            // Wait for ESM reject cause to be reported
            lwm2m_os_sleep(10);
            timeout_ms -= 10;
        }

        *esm_code = at_esm_error_code_get(cid);

        nrf_close(*fd);
        *fd = DEFAULT_PDN_FD;
        return -1;
    }

    /* PDN is active, but fd might have changed */
    return 1;
}

bool lwm2m_pdn_first_enabled_apn_instance(void)
{
    lwm2m_instance_t * p_instance;

    for (m_apn_instance = 0; m_apn_instance < LWM2M_MAX_APN_COUNT; m_apn_instance++) {
        if (lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, m_apn_instance) != 0) {
            continue;
        }

        if (lwm2m_apn_conn_prof_is_enabled(m_apn_instance)) {
            break;
        }
    }

    return (m_apn_instance == LWM2M_MAX_APN_COUNT) ? false : true;
}

bool lwm2m_pdn_next_enabled_apn_instance(void)
{
    lwm2m_instance_t * p_instance;
    bool instance_wrap = false;

    while (m_apn_instance < LWM2M_MAX_APN_COUNT) {
        m_apn_instance++;

        if (lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, m_apn_instance) != 0) {
            continue;
        }

        if (lwm2m_apn_conn_prof_is_enabled(m_apn_instance)) {
            break;
        }
    }

    if (m_apn_instance >= LWM2M_MAX_APN_COUNT) {
        m_apn_instance = 0;
        instance_wrap = true;
    }

    return instance_wrap;
}

void lwm2m_pdn_init(void)
{
    if (at_read_default_apn(m_default_apn, sizeof(m_default_apn)) != 0) {
        LWM2M_ERR("Unable to read default APN");
    }
}

static nrf_sa_family_t pdn_type_allowed(int esm_error_code)
{
    nrf_sa_family_t allowed;

    switch (esm_error_code) {
    case 50: // PDN type IPv4 only allowed
        allowed = NRF_AF_INET;
        break;
    case 51: // PDN type IPv6 only allowed
        allowed = NRF_AF_INET6;
        break;
    case 57: // PDN type IPv4v6 only allowed
    default:
        allowed = 0;
        break;
    }

    return allowed;
}

/** PDN type allowed for default CID. */
nrf_sa_family_t lwm2m_pdn_type_allowed(void)
{
    int esm_error_code = at_esm_error_code_get(0);

    return pdn_type_allowed(esm_error_code);
}

/**@brief Setup PDN connection, if necessary */
bool lwm2m_pdn_activate(bool *p_pdn_activated, nrf_sa_family_t *p_pdn_type_allowed)
{
    lwm2m_carrier_apn_get(m_current_apn, sizeof(m_current_apn));
    LWM2M_INF("PDN setup: %s", lwm2m_os_log_strdup(m_current_apn));

    /* Register for packet domain events before activating PDN */
    at_apn_register_for_packet_events();

    int esm_error_code = 0;
    int rc = pdn_activate(&m_pdn_handle, m_current_apn, &esm_error_code);
    if (rc < 0) {
        if (lwm2m_retry_count_pdn_get() == 0) {
            /* Only report first activate reject when doing retries */
            /* TODO: Check how to handle this properly */
            if (esm_error_code == 0) {
                esm_error_code = 34; // Service option temporarily out of order
            }
            lwm2m_apn_conn_prof_activate(m_apn_instance, esm_error_code);
        }
        at_apn_unregister_from_packet_events();

        return false;
    }

    /* Store used PDN Context ID for later use */
    m_pdn_cid = pdn_cid_get(m_pdn_handle);

    /* PDN was active */
    if (rc == 0) {
        at_apn_unregister_from_packet_events();
        lwm2m_retry_delay_pdn_reset();
        return true;
    }

    LWM2M_INF("Activating %s", lwm2m_os_log_strdup(m_current_apn));
    lwm2m_apn_conn_prof_activate(m_apn_instance, 0);

    esm_error_code = at_esm_error_code_get(m_pdn_cid);
    if (esm_error_code != 50) {
        /* PDN was reactivated, wait for IPv6 */
        rc = at_apn_setup_wait_for_ipv6(&m_pdn_handle);

        if (rc) {
            at_apn_unregister_from_packet_events();

            return false;
        }
    }

    at_apn_unregister_from_packet_events();
    lwm2m_retry_delay_pdn_reset();

    *p_pdn_activated = true;
    *p_pdn_type_allowed = pdn_type_allowed(esm_error_code);

    return true;
}

/**@brief Disconnect carrier PDN connection. */
void lwm2m_pdn_deactivate(void)
{
    if (m_pdn_handle != DEFAULT_PDN_FD) {
        if (m_apn_instance != lwm2m_apn_conn_prof_default_instance()) {
            lwm2m_apn_conn_prof_deactivate(m_apn_instance);
        }
        nrf_close(m_pdn_handle);
        m_pdn_handle = DEFAULT_PDN_FD;
    }
}

void lwm2m_pdn_check_closed(void)
{
    if ((m_pdn_handle != DEFAULT_PDN_FD) && (m_pdn_cid > 0)) {
        /* PDN is used and CID is known, check if the PDN is deactivated */
        if (0 < at_cid_active_state(m_pdn_cid)) {
            /* PDN is deactivated, close it and retry later */
            lwm2m_pdn_deactivate();
        }
    }
}

uint16_t lwm2m_apn_instance(void)
{
    return m_apn_instance;
}

char *lwm2m_pdn_current_apn(void)
{
    return m_current_apn;
}

char *lwm2m_pdn_default_apn(void)
{
    return m_default_apn;
}
