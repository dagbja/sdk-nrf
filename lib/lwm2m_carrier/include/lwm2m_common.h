/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <nrf_socket.h>

#include <lwm2m_api.h>

/**@brief Helper function to get the access from an instance and a remote. */
uint32_t lwm2m_access_remote_get(uint16_t            * p_access,
                                 lwm2m_instance_t    * p_instance,
                                 struct nrf_sockaddr * p_remote);

/**@brief Helper function to set ACL on an instance. */
void lwm2m_set_instance_acl(lwm2m_instance_t     * p_instance,
                            uint16_t               default_access,
                            lwm2m_instance_acl_t * p_acl);

/**@brief Helper function to set default carrier ACL on an instance. */
void lwm2m_set_carrier_acl(lwm2m_instance_t * p_instance);
