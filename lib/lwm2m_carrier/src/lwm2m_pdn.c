/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <string.h>
#include <nrf_socket.h>
#include <at_interface.h>

#include <lwm2m.h>
#include <lwm2m_os.h>
#include <lwm2m_pdn.h>
#include <at_interface.h>


int lwm2m_pdn_activate(int *fd, const char *apn, int *esm_code)
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
        }
        else if (active) {
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

    const int cid = lwm2m_pdn_cid_get(*fd);

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

        if (esm_code)
            *esm_code = at_esm_error_code_get(cid);

        nrf_close(*fd);
        *fd = DEFAULT_PDN_FD;
        return -1;
    }

    /* PDN is active, but fd might have changed */
    return 1;
}

int lwm2m_pdn_cid_get(int fd)
{
    int8_t cid = -1;

    if (fd >= 0) {
        nrf_socklen_t len = sizeof(cid);
        int err = nrf_getsockopt(fd, NRF_SOL_PDN, NRF_SO_PDN_CONTEXT_ID,
                                 &cid, &len);
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

int lwm2m_pdn_esm_error_code_get(int fd)
{
    const int cid = lwm2m_pdn_cid_get(fd);
    if (cid < 0) {
        return -1;
    }
    return at_esm_error_code_get(cid);
}

int lwm2m_pdn_esm_error_code_reset(int fd)
{
    const int cid = lwm2m_pdn_cid_get(fd);
    if (cid < 0) {
        return -1;
    }
    return at_esm_error_code_reset(cid);
}
