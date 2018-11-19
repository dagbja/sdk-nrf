/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file lwm2m_objects_tlv.h
 *
 * @defgroup iot_sdk_lwm2m_objects_tlv OMA LWM2M object TLV encoder and decoder API
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief OMA LWM2M object TLV encoder and decoder API.
 */

#ifndef LWM2M_OBJECTS_TLV_H__
#define LWM2M_OBJECTS_TLV_H__

#include <stdint.h>
#include <lwm2m_objects.h>
#include <lwm2m_tlv.h>

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Encode a LwM2M instance object to a TLV byte buffer.
 *
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided.
 *                            Will return the number of used bytes on return.
 * @param[in]    p_instance   Pointer to the LwM2M instance object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successful.
 */
uint32_t lwm2m_tlv_instance_encode(uint8_t          * p_buffer,
                                   uint32_t         * p_buffer_len,
                                   lwm2m_instance_t * p_instance);

/**@brief Decode a LWM2M security object from a TLV byte buffer.
 *
 * @note    Resource values NOT found in the TLV will not be altered.
 *
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_security Pointer to a LWM2M server object to be filled by the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 * @param[in]  resource_callback Callback function to handle vendor specific TLV resources.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_tlv_security_decode(lwm2m_security_t   * p_security,
                                   uint8_t            * p_buffer,
                                   uint32_t             buffer_len,
                                   lwm2m_tlv_callback_t resource_callback);

/**@brief Encode a LWM2M security object to a TLV byte buffer.
 *
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided.
 *                            Will return the number of used bytes on return.
 * @param[in]    resource_id  Resource identifier to encode. LWM2M_NAMED_OBJECT to encode all.
 * @param[in]    p_security   Pointer to the LWM2M security object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successful.
 */
uint32_t lwm2m_tlv_security_encode(uint8_t          * p_buffer,
                                   uint32_t         * p_buffer_len,
                                   uint16_t           resource_id,
                                   lwm2m_security_t * p_security);

/**@brief Decode a LWM2M server object from a TLV byte buffer.
 *
 * @note    Resource values NOT found in the TLV will not be altered.
 *
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_server   Pointer to a LWM2M server object to be filled by the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 * @param[in]  resource_callback Callback function to handle vendor specific TLV resources.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_tlv_server_decode(lwm2m_server_t     * p_server,
                                 uint8_t            * p_buffer,
                                 uint32_t             buffer_len,
                                 lwm2m_tlv_callback_t resource_callback);

/**@brief Encode a LWM2M server object to a TLV byte buffer.
 *
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided.
 *                            Will return the number of used bytes on return.
 * @param[in]    resource_id  Resource identifier to encode. LWM2M_NAMED_OBJECT to encode all.
 * @param[in]    p_server     Pointer to the LWM2M server object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successful.
 */
uint32_t lwm2m_tlv_server_encode(uint8_t        * p_buffer,
                                 uint32_t       * p_buffer_len,
                                 uint16_t         resource_id,
                                 lwm2m_server_t * p_server);

/**@brief Decode a LWM2M connectivity monitoring object from a TLV byte buffer.
 *
 * @note    Resource values NOT found in the TLV will not be altered.
 *
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_conn_mon Pointer to a LWM2M connectivity monitoring object to be filled by
 *                        the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 * @param[in]  resource_callback Callback function to handle vendor specific TLV resources.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_tlv_connectivity_monitoring_decode(lwm2m_connectivity_monitoring_t * p_conn_mon,
                                                  uint8_t                         * p_buffer,
                                                  uint32_t                          buffer_len,
                                                  lwm2m_tlv_callback_t              resource_callback);

/**@brief Encode a LWM2M connectivity monitoring object to a TLV byte buffer.
 *
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided.
 *                            Will return the number of used bytes on return.
 * @param[in]    resource_id  Resource identifier to encode. LWM2M_NAMED_OBJECT to encode all.
 * @param[in]    p_conn_mon   Pointer to the LWM2M connectivity monitoring object to be
 *                            encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successful.
 */
uint32_t lwm2m_tlv_connectivity_monitoring_encode(uint8_t                         * p_buffer,
                                                  uint32_t                        * p_buffer_len,
                                                  uint16_t                          resource_id,
                                                  lwm2m_connectivity_monitoring_t * p_conn_mon);

/**@brief Decode a LWM2M device object from a TLV byte buffer.
 *
 * @note    Resource values NOT found in the TLV will not be altered.
 *
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_device   Pointer to a LWM2M device object to be filled by
 *                        the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 * @param[in]  resource_callback Callback function to handle vendor specific TLV resources.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_tlv_device_decode(lwm2m_device_t     * p_device,
                                 uint8_t            * p_buffer,
                                 uint32_t             buffer_len,
                                 lwm2m_tlv_callback_t resource_callback);

/**@brief Encode a LWM2M device object to a TLV byte buffer.
 *
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided.
 *                            Will return the number of used bytes on return.
 * @param[in]    resource_id  Resource identifier to encode. LWM2M_NAMED_OBJECT to encode all.
 * @param[in]    p_device     Pointer to the LWM2M device object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successful.
 */
uint32_t lwm2m_tlv_device_encode(uint8_t        * p_buffer,
                                 uint32_t       * p_buffer_len,
                                 uint16_t         resource_id,
                                 lwm2m_device_t * p_device);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_OBJECTS_TLV_H__

/** @} */
