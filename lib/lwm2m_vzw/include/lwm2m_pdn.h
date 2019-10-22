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

#ifndef LWM2M_PDN_H_
#define LWM2M_PDN_H_

/**@brief Connect to a packet data network.
 *
 * Creates a PDN socket and connect to an access point, if necessary.
 *
 * @param[in,out]	fd	The socket handle.
 * @param[in]		apn	The packet data network name.
 *
 * @return 0 if PDN socket was valid and already connected.
 * @return 1 if PDN socket has been recreated.
 * @return -1 on error.
 */
int lwm2m_pdn_activate(int *fd, const char *apn);

#endif /* LWM2M_PDN_H_ */

