/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <nrf_socket.h>

#include <lwm2m_api.h>

/**@brief Helper function to get the access from an instance and a remote. */
uint32_t common_lwm2m_access_remote_get(uint16_t            *p_access,
                                        lwm2m_instance_t    *p_instance,
                                        struct nrf_sockaddr *p_remote);
