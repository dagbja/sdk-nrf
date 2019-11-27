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

int lwm2m_pdn_activate(int *fd, const char *apn)
{
	int err;
	nrf_pdn_state_t active = 0;
	nrf_socklen_t len = sizeof(active);

	if (fd == NULL || apn == NULL) {
		LWM2M_ERR("PDN activation - invalid params: fd (%d) apn (%d)", fd, (int)apn);
		return -1;
	}

	if (*fd != -1) {
		/* If handle is valid, check if PDN is active */
		err = nrf_getsockopt(*fd, NRF_SOL_PDN, NRF_SO_PDN_STATE, &active, &len);
		if (err) {
			LWM2M_ERR("Reading PDN state failed: %s (%d)",
                  lwm2m_os_log_strdup(strerror(lwm2m_os_errno())), lwm2m_os_errno());
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
                  lwm2m_os_log_strdup(strerror(lwm2m_os_errno())), lwm2m_os_errno());
		return -1;
	}

	/* Connect to the PDN. */
	err = nrf_connect(*fd, (struct nrf_sockaddr *)apn, strlen(apn));
	if (err) {
		LWM2M_ERR("PDN connect failed: %s (%d)",
                  lwm2m_os_log_strdup(strerror(lwm2m_os_errno())), lwm2m_os_errno());
		nrf_close(*fd);
		*fd = -1;
		return -1;
	}

	/* PDN is active, but fd might have changed */
	return 1;
}
