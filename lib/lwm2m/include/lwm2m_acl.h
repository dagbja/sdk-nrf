/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m_acl.h
 *
 * @defgroup iot_sdk_lwm2m_acl_api LWM2M ACL API interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief ACL API interface for the LWM2M protocol.
 */

#ifndef LWM2M_ACL_H__
#define LWM2M_ACL_H__

#include <lwm2m_api.h>

#define LWM2M_ACL_NO_PERM                    0
#define LWM2M_ACL_FULL_PERM                  (LWM2M_PERMISSION_READ    | \
                                              LWM2M_PERMISSION_WRITE   | \
                                              LWM2M_PERMISSION_EXECUTE | \
                                              LWM2M_PERMISSION_DELETE  | \
                                              LWM2M_PERMISSION_CREATE)
#define LWM2M_ACL_DEFAULT_SHORT_SERVER_ID    0
#define LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID  65535
#define LWM2M_ACL_LIST_TLV_SIZE              (LWM2M_MAX_SERVERS * 5) /* Bytes required to serialize an ACL list to TLV. */
#define LWM2M_ACL_RESOURCES_TLV_SIZE         15                      /* Bytes required to serialize all resources except the ACL list to TLV. */
#define LWM2M_ACL_TLV_SIZE                   (LWM2M_ACL_LIST_TLV_SIZE + \
                                              LWM2M_ACL_RESOURCES_TLV_SIZE)

/**
 * @brief      Initialize this module.
 * @details    Calling this method will set the module in its default state.
 *
 * @return     NRF_SUCCESS Initialization succeeded.
 */
uint32_t lwm2m_acl_init(void);

/**
 * @brief Initialize the access control list of the instance, and give the ACL a unique id.
 *
 * @param[in]       p_instance  Instance to Initialize.
 * @param[in]       owner       Owner of the instance (short server id).
 *
 * @retval NRF_SUCCESS    Initialization succeeded.
 * @retval NRF_ERROR_NULL The p_instance parameter was NULL.
 */
uint32_t lwm2m_acl_permissions_init(lwm2m_instance_t * p_instance, uint16_t owner);

/**
 * @brief Check server access permissions on an object instance.
 *
 * @param[out] p_access         Access, see lwm2m_api.h for op code masks (Discover and Observe is not valid for ACL).
 * @param[in]  p_instance       Instance to check access rights on.
 * @param[in]  short_server_id  Short server id of the server to check access of.
 *
 * @retval NRF_SUCCESS    Access successfully retrieved.
 * @retval NRF_ERROR_NULL The p_instance parameter was NULL.
 */
uint32_t lwm2m_acl_permissions_check(uint16_t         * p_access,
                                     lwm2m_instance_t * p_instance,
                                     uint16_t           short_server_id);

/**
 * @brief Add permissions to a given short server id.
 *
 * @param[in]       p_instance       Instance to set access rights on.
 * @param[in]       access           Access rights to set. See lwm2m_api.h for op code masks (Discover and Observe is not valid for ACL).
 * @param[in]       short_server_id  Short server id of the server to grant access.
 *
 * @retval NRF_SUCCESS      Access successfully added.
 * @retval NRF_ERROR_NO_MEM Access control list is full.
 * @retval NRF_ERROR_NULL   The p_instance parameter was NULL.
 */
uint32_t lwm2m_acl_permissions_add(lwm2m_instance_t * p_instance,
                                   uint16_t           access,
                                   uint16_t           short_server_id);

/**
 * @brief Removes all access rights granted for a server.
 *
 * @param[in]       p_instance       Instance to remove access rights from.
 * @param[in]       short_server_id  Short server id of the server to revoke access for.
 *
 * @retval NRF_SUCCESS    Access successfully removed.
 * @retval NRF_ERROR_NULL The p_instance parameter was NULL.
 */
uint32_t lwm2m_acl_permissions_remove(lwm2m_instance_t * p_instance,
                                      uint16_t           short_server_id);

/**
 * @brief Reset the access control list of the instance, keep the ACL unique id.
 *
 * @param[in] p_instance  Instance to reset.
 * @param[in] owner       Owner of the instance (short server id).
 *
 * @retval NRF_SUCCESS    Reset succeeded.
 * @retval NRF_ERROR_NULL The p_instance parameter was NULL.
 */
uint32_t lwm2m_acl_permissions_reset(lwm2m_instance_t * p_instance,
                                     uint16_t           owner);

/**
 * @brief Serialize the ACL of the instance into TLV.
 *
 * @param[out] p_buffer     Buffer to serialize the ACL instance into.
 * @param[out] p_buffer_len Length of the buffer. Will be return the number of bytes used if
 *                          serialization succeeded.
 * @param[in]  p_instance   Instance to serialize.
 *
 * @retval NRF_SUCCESS      Serialization succeeded.
 * @retval NRF_ERROR_NO_MEM Buffer not large enough to serialize the instance.
 */
uint32_t lwm2m_acl_serialize_tlv(uint8_t          * p_buffer,
                                 uint32_t         * p_buffer_len,
                                 lwm2m_instance_t * p_instance);

/**
 * @brief Deserialize an ACL from a TLV.
 *
 * @param[out] p_buffer     Buffer to deserialize the ACL from.
 * @param[in]  buffer_len   Length of the buffer.
 * @param[in]  p_instance   Instance that the ACL belongs to or NULL
 *                          to deserialize automatically into the right instance.
 *
 * @retval NRF_SUCCESS      Deserialization succeeded.
 * @retval NRF_ENOENT       Instance could not be found.
 */
uint32_t lwm2m_acl_deserialize_tlv(uint8_t          * p_buffer,
                                   uint16_t           buffer_len,
                                   lwm2m_instance_t * p_instance);

#endif // LWM2M_ACL_H__

/**@} */
