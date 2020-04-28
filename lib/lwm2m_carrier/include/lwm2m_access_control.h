/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_ACCESS_CONTROL_H__
#define LWM2M_ACCESS_CONTROL_H__

#include <stdint.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

lwm2m_access_control_t * lwm2m_access_control_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_access_control_get_object(void);

/**@brief Helper function to get the access from an object or object instance and a remote. */
uint32_t lwm2m_access_control_access_remote_get(uint16_t            * p_access,
                                                uint16_t              object_id,
                                                uint16_t              instance_id,
                                                struct nrf_sockaddr * p_remote);

/**@brief Bind an Access Control instance to the given object or object instance. The ID of the
 *        corresponding Access Control instance will be returned via @p p_access_control_id (if it is not NULL). */
uint32_t lwm2m_access_control_instance_bind(uint16_t object_id, uint16_t instance_id, uint16_t *p_access_control_id);

/**@brief Unbind the Access Control instance matching the given object or object instance. */
void lwm2m_access_control_instance_unbind(uint16_t object_id, uint16_t instance_id);

/**@brief Set ACL on the Access Control instance binded to the given object or object instance
 *        (if there is no matching instance, a new one is binded). */
void lwm2m_access_control_acl_set(uint16_t object_id, uint16_t instance_id, const lwm2m_list_t *p_acl);

/**@brief Set carrier-specific ACL on the Access Control instance binded to the given object or object instance. */
void lwm2m_access_control_carrier_acl_set(uint16_t object_id, uint16_t instance_id);

/**@brief Set the owner of the Access Control instance binded to the given object or object instance. */
void lwm2m_access_control_owner_set(uint16_t object_id, uint16_t instance_id, uint16_t owner);

/**@brief Initialise the Access Control object and all its instances. */
void lwm2m_access_control_init(void);

/**@brief Initialise Access Control instances for all the object instances registered in the CoAP handler
 *        according to carrier-specific requirements. */
void lwm2m_access_control_acl_init(void);

/**@brief Unbind and clear all the existing Access Control instances. */
void lwm2m_access_control_delete_instances(void);

/**@brief Find the instance ID of the Access Control instance binded to the given object or object instance. */
uint32_t lwm2m_access_control_find(uint16_t object_id, uint16_t instance_id, uint16_t *p_access_control_id);

#endif /* LWM2M_ACCESS_CONTROL_H__ */
