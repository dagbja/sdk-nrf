/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m_tlv.h
 *
 * @defgroup iot_sdk_lwm2m_tlv_api LWM2M TLV interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief TLV encoding and decoding interface for the LWM2M protocol.
 */

#ifndef LWM2M_TLV_H__
#define LWM2M_TLV_H__

#include <stdint.h>
#include <lwm2m_objects.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TLV type masks
 */
#define TLV_TYPE_BIT_POS           6
#define TLV_ID_LEN_BIT_POS         5
#define TLV_LEN_TYPE_BIT_POS       3
#define TLV_VAL_LEN_BIT_POS        0

#define TLV_TYPE_MASK              (0x3 << TLV_TYPE_BIT_POS)     /**< Type bitmask, bit 7-6                (0b11000000). */
#define TLV_ID_LEN_MASK            (0x1 << TLV_ID_LEN_BIT_POS)   /**< Length bitmask, bit 5                (0b00100000). */
#define TLV_LEN_TYPE_MASK          (0x3 << TLV_LEN_TYPE_BIT_POS) /**< Length type bitmask, bit 4-3         (0b00011000). */
#define TLV_LEN_VAL_MASK           (0x7 << TLV_VAL_LEN_BIT_POS)  /**< Length of the value bitmask, bit 2-0 (0b00000111). */

#define TLV_TYPE_OBJECT            0x00
#define TLV_TYPE_RESOURCE_INSTANCE 0x01
#define TLV_TYPE_MULTI_RESOURCE    0x02
#define TLV_TYPE_RESOURCE_VAL      0x03

#define TLV_ID_LEN_8BIT            0x00
#define TLV_ID_LEN_16BIT           0x01

#define TLV_LEN_TYPE_3BIT          0x00
#define TLV_LEN_TYPE_8BIT          0x01
#define TLV_LEN_TYPE_16BIT         0x02
#define TLV_LEN_TYPE_24BIT         0x03

typedef struct
{
    uint16_t  id_type; /**< Identifier type. */
    uint16_t  id;      /**< Identifier ID. */
    uint32_t  length;  /**< Length of the value in the TLV. */
    uint8_t * value;   /**< Value of the TLV. */
} lwm2m_tlv_t;

/**@brief Decode a LWM2M TLV byte buffer into a TLV structure.
 *
 * @param[out]   p_tlv        This struct will be filled with id, length, type and pointer to value.
 * @param[inout] p_index      Index to start decoding from.
 * @param[in]    p_buffer     The buffer to decode from.
 * @param[in]    buffer_len   The length of the buffer.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 * @retval IOT_LWM2M_ERR_BASE | NRF_INVALID_DATA
 */
uint32_t lwm2m_tlv_decode(lwm2m_tlv_t * p_tlv,
                          uint32_t    * p_index,
                          uint8_t     * p_buffer,
                          uint16_t      buffer_len);

/**@brief Encode a TLV structure into a LWM2M TLV header byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV into.
 * @param[inout] p_buffer_len  Length of input buffer out: length of the encoded buffer.
 * @param[in]    p_tlv         The TLV to use.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 * @retval IOT_LWM2M_ERR_BASE | NRF_ERROR_DATA_SIZE
 */
uint32_t lwm2m_tlv_header_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, lwm2m_tlv_t * p_tlv);

/**@brief Encode a TLV structure into a LWM2M TLV byte buffer.
 *
 * @details Encode using the provided TLV, if the buffer provided is to small an NRF_ERROR_DATA_SIZE will be returned.
 *
 * Maximum buffer size requirement: value_length + 6 (1 for type byte, 2 for id bytes, 3 for length bytes).
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV into.
 * @param[inout] p_buffer_len  Length of input buffer out: length of the encoded buffer.
 * @param[in]    p_tlv         The TLV to use.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 * @retval IOT_LWM2M_ERR_BASE | NRF_ERROR_DATA_SIZE
 */
uint32_t lwm2m_tlv_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, lwm2m_tlv_t * p_tlv);

/**@brief Decode a LwM2M TLV list byte buffer into a TLV list structure.
 *
 * @param[in]  tlv_range TLV containing the list of TLVs.
 * @param[out] p_list    List of decoded values.
 *
 * @retval NRF_SUCCESS If decoding was successful.
 */
uint32_t lwm2m_tlv_list_decode(lwm2m_tlv_t tlv_range, lwm2m_list_t * p_list);

/**@brief Encode a TLV list structure into a LwM2M TLV list byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV list into.
 * @param[inout] p_buffer_len  Length of input buffer out: remaining length of buffer.
 * @param[in]    id            TLV Identifier ID.
 * @param[in]    p_list        List of values to encode.
 *
 * @retval NRF_SUCCESS If encoding was successful.
 */
uint32_t lwm2m_tlv_list_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, const lwm2m_list_t * p_list);

/**@brief Encode a TLV string into a LwM2M TLV list byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV list into.
 * @param[inout] p_buffer_len  Length of input buffer out: remaining length of buffer.
 * @param[in]    id            TLV Identifier ID.
 * @param[in]    value         Value to encode.
 *
 * @retval NRF_SUCCESS If encoding was successful.
 */
uint32_t lwm2m_tlv_string_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, lwm2m_string_t value);

/**@brief Encode a TLV integer into a LwM2M TLV list byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV list into.
 * @param[inout] p_buffer_len  Length of input buffer out: remaining length of buffer.
 * @param[in]    id            TLV Identifier ID.
 * @param[in]    value         Value to encode.
 *
 * @retval NRF_SUCCESS If encoding was successful.
 */
uint32_t lwm2m_tlv_integer_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, int32_t value);

/**@brief Encode a TLV bool into a LwM2M TLV list byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV list into.
 * @param[inout] p_buffer_len  Length of input buffer out: remaining length of buffer.
 * @param[in]    id            TLV Identifier ID.
 * @param[in]    value         Value to encode.
 *
 * @retval NRF_SUCCESS If encoding was successful.
 */
uint32_t lwm2m_tlv_bool_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, bool value);

/**@brief Encode a TLV opaque into a LwM2M TLV list byte buffer.
 *
 * @param[out]   p_buffer      Buffer to put the encoded TLV list into.
 * @param[inout] p_buffer_len  Length of input buffer out: remaining length of buffer.
 * @param[in]    id            TLV Identifier ID.
 * @param[in]    value         Value to encode.
 *
 * @retval NRF_SUCCESS If encoding was successful.
 */
uint32_t lwm2m_tlv_opaque_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, lwm2m_opaque_t value);

/**@brief Encode a byte buffer into a int32_t.
 *
 * @param[in]  p_buffer Buffer which holds a serialized version of the int32_t.
 * @param[in]  val_len  Length of the value in the buffer.
 * @param[out] p_result By reference pointer to the result int32_t.
 *
 * @retval NRF_SUCCESS If the conversion from byte buffer to int32_t value was successful.
 */
uint32_t lwm2m_tlv_bytebuffer_to_int32(uint8_t * p_buffer, uint8_t val_len, int32_t * p_result);

/**@brief Encode a byte buffer into a uint16_t.
 *
 * @param[in]  p_buffer Buffer which holds a serialized version of the uint16_t.
 * @param[in]  val_len  Length of the value in the buffer.
 * @param[out] p_result By reference pointer to the result uint16_t.
 *
 * @retval NRF_SUCCESS If the conversion from byte buffer to uint16_t value was successful.
 */
uint32_t lwm2m_tlv_bytebuffer_to_uint16(uint8_t * p_buffer, uint8_t val_len, uint16_t * p_result);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_TLV_H__

/** @} */
