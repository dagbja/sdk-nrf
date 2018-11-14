/*$$$LICENCE_NORDIC_STANDARD<2018>$$$*/
/**@file lwm2m_objects_plain_text.h
 *
 * @defgroup iot_sdk_lwm2m_objects_plain_text OMA LWM2M object plain text encoder and decoder API
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief OMA LWM2M object plain text encoder and decoder API.
 */

#ifndef LWM2M_OBJECTS_PLAIN_TEXT_H__
#define LWM2M_OBJECTS_PLAIN_TEXT_H__

#include <stdint.h>
#include "lwm2m_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Decode a LWM2M server resource from a plain text byte buffer.
 *
 * @param[out] p_server    Pointer to a LWM2M server object to be filled by the resource.
 * @param[in]  resource_id Resource identifier to decode.
 * @param[in]  p_buffer    Pointer to the byte buffer to be decoded.
 * @param[in]  buffer_len  Size of the buffer to be decoded.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_plain_text_server_decode(lwm2m_server_t * p_server,
                                        uint16_t         resource_id,
                                        uint8_t        * p_buffer,
                                        uint32_t         buffer_len);

/**@brief Decode a LWM2M device resource from a plain text byte buffer.
 *
 * @param[out] p_device    Pointer to a LWM2M device object to be filled by the resource.
 * @param[in]  resource_id Resource identifier to decode.
 * @param[in]  p_buffer    Pointer to the byte buffer to be decoded.
 * @param[in]  buffer_len  Size of the buffer to be decoded.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_plain_text_device_decode(lwm2m_device_t * p_device,
                                        uint16_t         resource_id,
                                        uint8_t        * p_buffer,
                                        uint32_t         buffer_len);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_OBJECTS_PLAIN_TEXT_H__

/** @} */
