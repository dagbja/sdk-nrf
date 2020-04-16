/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**
 * @file lwm2m_pdn.h
 *
 * @brief API for the PDN management.
 */

#ifndef LWM2M_PDN_H__
#define LWM2M_PDN_H__

/**
 * @brief PDN Context ID max value.
 */
#define MAX_NUM_OF_PDN_CONTEXTS (25)

/**
 * @brief Socket descriptor value used as a default PDN.
 */
#define DEFAULT_PDN_FD (-1)

/**@brief Connect to a packet data network.
 *
 * Creates a PDN socket and connect to an access point, if necessary.
 *
 * @param[in,out]	fd			The socket handle.
 * @param[in]		apn			The packet data network name.
 * @param[out]		esm_code	ESM error code in case of failure if not NULL.
 *
 * @return 0 if PDN socket was valid and already connected.
 * @return 1 if PDN socket has been recreated.
 * @return -1 on error.
 */
int lwm2m_pdn_activate(int *fd, const char *apn, int *esm_code);

/** @brief Get PDN Context ID (CID) of the socket.
 *
 * @param[in]	fd	The socket handle.
 *
 * @return CID or -1 on error.
 */
int lwm2m_pdn_cid_get(int fd);

/** @brief Get ESM error code by socket handle.
 *
 * @param[in]	fd	The socket handle.
 *
 * @return ESM code or -1 on error.
 */
int lwm2m_pdn_esm_error_code_get(int fd);

/** @brief Reset ESM error code by socket handle.
 *
 * @param[in]	fd	The socket handle.
 *
 * @return Zero on success, -1 otherwise.
 */
int lwm2m_pdn_esm_error_code_reset(int fd);


#endif /* LWM2M_PDN_H__ */
