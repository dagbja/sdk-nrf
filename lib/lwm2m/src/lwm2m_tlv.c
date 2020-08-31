/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>


uint32_t lwm2m_tlv_bytebuffer_to_int32(uint8_t * p_buffer, uint8_t val_len, int32_t * p_result)
{
    int32_t res;

    switch (val_len)
    {
        case 0:
        {
            res = 0;
            break;
        }

        case 1:
        {
            res = p_buffer[0];
            break;
        }

        case 2:
        {
            res = ((int32_t)p_buffer[0] << 8) |
                  p_buffer[1];
            break;
        }

        case 4:
        {
            res = ((int32_t)p_buffer[0] << 24) |
                  ((int32_t)p_buffer[1] << 16) |
                  ((int32_t)p_buffer[2] << 8) |
                  p_buffer[3];
            break;
        }

        default:
            return EMSGSIZE;
    }

    *p_result = res;
    return 0;
}


static uint32_t lwm2m_tlv_bytebuffer_to_uint32(uint8_t * p_buffer, uint8_t val_len, uint32_t * p_result)
{
    uint32_t res;

    switch (val_len)
    {
        case 0:
        {
            res = 0;
            break;
        }

        case 1:
        {
            res = p_buffer[0];
            break;
        }

        case 2:
        {
            res = ((uint32_t)p_buffer[0] << 8) |
                  p_buffer[1];
            break;
        }

        case 3:
        {
            res = ((uint32_t)p_buffer[0] << 16) |
                  ((uint32_t)p_buffer[1] << 8) |
                  p_buffer[2];
            break;
        }

        case 4:
        {
            res = ((uint32_t)p_buffer[0] << 24) |
                  ((uint32_t)p_buffer[1] << 16) |
                  ((uint32_t)p_buffer[2] << 8) |
                  p_buffer[3];
            break;
        }

        default:
            return EMSGSIZE;
    }

    *p_result = res;
    return 0;
}


uint32_t lwm2m_tlv_bytebuffer_to_uint16(uint8_t * p_buffer, uint8_t val_len, uint16_t * p_result)
{
    uint32_t err;
    uint32_t res;

    /* uint16_t types are encoded as int32_t types, since there is no
     * unsigned type in LwM2M, we encode it as a 4 byte integer.
     * Here, we decode it as such and then cast it to a uint16_t.
     */
    err = lwm2m_tlv_bytebuffer_to_uint32(p_buffer, val_len, &res);

    *p_result = res;

    return err;
}


static uint8_t lwm2m_tlv_integer_length(int32_t value)
{
    uint8_t length = 0;

    if (value <= INT8_MAX && value >= INT8_MIN)
    {
        length = 1;
    }
    else if (value <= INT16_MAX && value >= INT16_MIN)
    {
        length = 2;
    }
    else
    {
        length = 4;
    }

    return length;
}


static void lwm2m_tlv_uint32_to_bytebuffer(uint8_t * p_buffer, uint8_t * p_len, uint32_t value)
{
    if (value <= UINT8_MAX)
    {
        p_buffer[0] = value;
        *p_len      = 1;
    }
    else if (value <= UINT16_MAX)
    {
        p_buffer[1] = value;
        p_buffer[0] = value >> 8;
        *p_len      = 2;
    }
    else if (value <= 0xFFFFFF) // 24 bit
    {
        p_buffer[2] = value;
        p_buffer[1] = value >> 8;
        p_buffer[0] = value >> 16;
        *p_len      = 3;
    }
    else
    {
        p_buffer[3] = value;
        p_buffer[2] = value >> 8;
        p_buffer[1] = value >> 16;
        p_buffer[0] = value >> 24;
        *p_len      = 4;
    }
}


static void lwm2m_tlv_int32_to_bytebuffer(uint8_t * p_buffer, uint8_t * p_len, int32_t value)
{
    if (value <= INT8_MAX && value >= INT8_MIN)
    {
        p_buffer[0] = value;
        *p_len      = 1;
    }
    else if (value <= INT16_MAX && value >= INT16_MIN)
    {
        p_buffer[1] = value;
        p_buffer[0] = value >> 8;
        *p_len      = 2;
    }
    else
    {
        p_buffer[3] = value;
        p_buffer[2] = value >> 8;
        p_buffer[1] = value >> 16;
        p_buffer[0] = value >> 24;
        *p_len      = 4;
    }
}


static uint32_t lwm2m_tlv_list_item_length(const lwm2m_list_t * p_list,
                                           uint32_t             instance,
                                           uint32_t           * p_len)
{
    switch (p_list->type)
    {
        case LWM2M_LIST_TYPE_UINT8:
            *p_len = lwm2m_tlv_integer_length(p_list->val.p_uint8[instance]);
            break;

        case LWM2M_LIST_TYPE_UINT16:
            *p_len = lwm2m_tlv_integer_length(p_list->val.p_uint16[instance]);
            break;

        case LWM2M_LIST_TYPE_INT32:
            *p_len = lwm2m_tlv_integer_length(p_list->val.p_int32[instance]);
            break;

        case LWM2M_LIST_TYPE_STRING:
            *p_len = p_list->val.p_string[instance].len;
            break;

        default:
            return EINVAL;
    }

    return 0;
}


static uint32_t lwm2m_tlv_list_item_encode(uint8_t            * p_buffer,
                                           uint32_t           * p_buffer_len,
                                           const lwm2m_list_t * p_list,
                                           uint32_t             instance)
{
    uint8_t value_buffer[8];

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_INSTANCE,
    };

    if (p_list->p_id)
    {
        tlv.id = p_list->p_id[instance];
    }
    else
    {
        tlv.id = instance;
    }

    if (p_list->type == LWM2M_LIST_TYPE_STRING)
    {
        tlv.length = p_list->val.p_string[instance].len;
        tlv.value  = (uint8_t *)p_list->val.p_string[instance].p_val;
    }
    else
    {
        int32_t value;
        uint8_t value_length;

        switch (p_list->type)
        {
            case LWM2M_LIST_TYPE_UINT8:
                value = p_list->val.p_uint8[instance];
                break;

            case LWM2M_LIST_TYPE_UINT16:
                value = p_list->val.p_uint16[instance];
                break;

            case LWM2M_LIST_TYPE_INT32:
                value = p_list->val.p_int32[instance];
                break;

            default:
                return EINVAL;
        }

        lwm2m_tlv_int32_to_bytebuffer(value_buffer, &value_length, value);

        tlv.length = value_length;
        tlv.value  = value_buffer;
    }

    return lwm2m_tlv_encode(p_buffer, p_buffer_len, &tlv);
}


uint32_t lwm2m_tlv_list_decode(lwm2m_tlv_t    tlv_range,
                               lwm2m_list_t * p_list)
{
    NULL_PARAM_CHECK(p_list);

    uint32_t    err_code;
    int32_t     value = 0;
    uint32_t    index = 0;

    lwm2m_tlv_t tlv;

    p_list->len = 0;

    while (index < tlv_range.length)
    {
        if (p_list->len >= p_list->max_len)
        {
            return EMSGSIZE;
        }

        err_code = lwm2m_tlv_decode(&tlv, &index, tlv_range.value, tlv_range.length);

        if (err_code != 0)
        {
            return err_code;
        }

        if (p_list->p_id)
        {
            p_list->p_id[p_list->len] = tlv.id;
        }

        switch (p_list->type)
        {
            case LWM2M_LIST_TYPE_UINT8:
            case LWM2M_LIST_TYPE_UINT16:
            case LWM2M_LIST_TYPE_INT32:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value, tlv.length, &value);

                if (err_code != 0)
                {
                    return err_code;
                }

                err_code = lwm2m_list_integer_append(p_list, value);

                if (err_code != 0)
                {
                    return err_code;
                }
                break;
            }

            case LWM2M_LIST_TYPE_STRING:
            {
                err_code = lwm2m_list_string_append(p_list, tlv.value, tlv.length);

                if (err_code != 0)
                {
                    return err_code;
                }
                break;
            }

            default:
                return EINVAL;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_list_encode(uint8_t            * p_buffer,
                               uint32_t           * p_buffer_len,
                               uint16_t             id,
                               const lwm2m_list_t * p_list)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_list);

    uint32_t max_buffer = *p_buffer_len;
    uint8_t header_buffer[6];
    uint32_t list_len = 0;
    uint32_t err_code;

    if (p_list->len == 0)
    {
        // Nothing to encode
        *p_buffer_len = 0;

        return 0;
    }

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_INSTANCE
    };

    // First calculate size of TLV_TYPE_MULTI_RESOURCE.
    for (uint32_t i = 0; i < p_list->len; ++i)
    {
        uint32_t header_buffer_len = 6;

        if (p_list->p_id)
        {
            tlv.id = p_list->p_id[i];
        }
        else
        {
            tlv.id = i;
        }

        err_code = lwm2m_tlv_list_item_length(p_list, i, &tlv.length);

        if (err_code != 0)
        {
            return err_code;
        }

        err_code = lwm2m_tlv_header_encode(header_buffer, &header_buffer_len, &tlv);

        if (err_code != 0)
        {
            return err_code;
        }

        list_len += (header_buffer_len + tlv.length);
    }

    tlv.id_type = TLV_TYPE_MULTI_RESOURCE;
    tlv.id      = id;
    tlv.length  = list_len;
    tlv.value   = NULL;

    uint32_t buffer_len = *p_buffer_len;
    err_code = lwm2m_tlv_header_encode(p_buffer, p_buffer_len, &tlv);

    if (err_code != 0)
    {
        return err_code;
    }

    // Progress buffer and adjust remaining size
    p_buffer += *p_buffer_len;
    *p_buffer_len = (buffer_len - *p_buffer_len);

    tlv.id_type = TLV_TYPE_RESOURCE_INSTANCE;

    // Then encode each resource.
    for (uint32_t i = 0; i < p_list->len; ++i)
    {
        buffer_len = *p_buffer_len;
        err_code = lwm2m_tlv_list_item_encode(p_buffer, p_buffer_len, p_list, i);

        if (err_code != 0)
        {
            return err_code;
        }

        // Progress buffer and adjust remaining size
        p_buffer += *p_buffer_len;
        *p_buffer_len = (buffer_len - *p_buffer_len);
    }

    // Set length of the output buffer.
    *p_buffer_len = (max_buffer - *p_buffer_len);

    return err_code;
}

uint32_t lwm2m_tlv_header_size_get(uint8_t * p_buffer)
{
    const uint8_t id_len       = (*p_buffer & TLV_ID_LEN_MASK) >> TLV_ID_LEN_BIT_POS;
    const uint8_t length_len   = (*p_buffer & TLV_LEN_TYPE_MASK) >> TLV_LEN_TYPE_BIT_POS;
    const uint8_t id_len_bytes = id_len + 1;

    return id_len_bytes + length_len + 1 /* type */;
}

uint32_t lwm2m_tlv_decode(lwm2m_tlv_t * p_tlv,
                          uint32_t    * p_index,
                          uint8_t     * p_buffer,
                          uint16_t      buffer_len)
{
    uint32_t err_code;
    uint16_t index = *p_index;

    uint8_t  type       = (p_buffer[index] & TLV_TYPE_MASK) >> TLV_TYPE_BIT_POS;
    uint8_t  id_len     = (p_buffer[index] & TLV_ID_LEN_MASK) >> TLV_ID_LEN_BIT_POS;
    uint8_t  length_len = (p_buffer[index] & TLV_LEN_TYPE_MASK) >> TLV_LEN_TYPE_BIT_POS;
    uint32_t length     = (p_buffer[index] & TLV_LEN_VAL_MASK) >> TLV_VAL_LEN_BIT_POS;

    p_tlv->id_type = type;
    p_tlv->length  = 0;

    // Jump to the byte following the "Type" at index 0.
    ++index;

    // Extract the Identifier based on the number of bytes indicated in id_len (bit 5).
    // Adding one to the id_len will give the number of bytes used.
    uint8_t id_len_size = id_len + 1;

    err_code = lwm2m_tlv_bytebuffer_to_uint16(&p_buffer[index], id_len_size, &p_tlv->id);
    if (err_code != 0)
    {
        return err_code;
    }

    index += id_len_size;

    // Extract the value length.
    // The length_len tells how many bytes are being used.
    if (length_len == TLV_LEN_TYPE_3BIT)
    {
        p_tlv->length = length;
    }
    else
    {
        err_code = lwm2m_tlv_bytebuffer_to_uint32(&p_buffer[index], length_len, &length);
        if (err_code != 0)
        {
            return err_code;
        }

        p_tlv->length = length;
        index        += length_len;
    }

    if (p_tlv->length > buffer_len)
    {
        return ENOMEM;
    }

    p_tlv->value = &p_buffer[index];

    *p_index = index + p_tlv->length;

    return 0;
}


uint32_t lwm2m_tlv_header_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, lwm2m_tlv_t * p_tlv)
{
    uint8_t length_len;
    uint8_t id_len;

    uint8_t  id[2]  = {0, };
    // Length is at most 3 bytes, we declare it 4 bytes long to prevent
    // a compiler warning when passing it to lwm2m_tlv_uint32_to_bytebuffer().
    uint8_t  len[4] = {0, };
    uint16_t index  = 0;
    uint8_t  type   = 0;

    // Set Identifier type by copying the lwm2m_tlv_t->id_type into bit 7-6.
    type = (p_tlv->id_type << TLV_TYPE_BIT_POS);

    // Set length of Identifier in bit 5 in the TLV type byte.
    if (p_tlv->id > UINT8_MAX)
    {
        type  |= (TLV_ID_LEN_16BIT << TLV_ID_LEN_BIT_POS);
        id[0]  = p_tlv->id >> 8;
        id[1]  = p_tlv->id;
        id_len = 2;
    }
    else
    {
        type  |= (TLV_ID_LEN_8BIT << TLV_ID_LEN_BIT_POS);
        id[0]  = p_tlv->id;
        id_len = 1;
    }

    // Set type of Length bit 4-3 in the TLV type byte.

    // If the Length can fit into 3 bits.
    if ((p_tlv->length & TLV_LEN_VAL_MASK) == p_tlv->length)
    {
        type      |= (TLV_LEN_TYPE_3BIT << TLV_LEN_TYPE_BIT_POS);
        length_len = 0;

        // As Length type field is set to "No Length", set bit 2-0.
        type |= (p_tlv->length & TLV_LEN_VAL_MASK);
    }
    else
    {
        lwm2m_tlv_uint32_to_bytebuffer(len, &length_len, p_tlv->length);

        // Length can not be larger than 24-bit.
        if (length_len > TLV_LEN_TYPE_24BIT)
        {
            return EINVAL;
        }

        type |= (length_len << TLV_LEN_TYPE_BIT_POS);
    }

    // Check if the buffer is large enough.
    if (*p_buffer_len < (id_len + length_len + 1)) // + 1 for the type byte
    {
        return EMSGSIZE;
    }

    // Copy the type to the buffer.
    memcpy(p_buffer + index, &type, 1);
    ++index;

    // Copy the Identifier to the buffer.
    memcpy(p_buffer + index, id, id_len);
    index += id_len;

    // Copy length to the buffer.
    if (length_len != 0)
    {
        memcpy(p_buffer + index, len, length_len);
        index += length_len;
    }

    // Set length of the output buffer.
    *p_buffer_len = index;

    return 0;
}


uint32_t lwm2m_tlv_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, lwm2m_tlv_t * p_tlv)
{
    uint32_t header_len = *p_buffer_len;
    uint32_t err_code   = lwm2m_tlv_header_encode(p_buffer, &header_len, p_tlv);

    if (err_code != 0)
    {
        return err_code;
    }

    if (p_tlv->length > 0)
    {
        // Check if the buffer is large enough.
        if (*p_buffer_len < (header_len + p_tlv->length))
        {
            return EMSGSIZE;
        }

        // Copy the value to buffer.
        memcpy(p_buffer + header_len, p_tlv->value, p_tlv->length);
    }

    // Set length of the output buffer.
    *p_buffer_len = (header_len + p_tlv->length);

    return 0;
}


uint32_t lwm2m_tlv_string_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, lwm2m_string_t value)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_VAL,
        .id      = id,
        .length  = value.len,
        .value   = (uint8_t *)value.p_val
    };

    return lwm2m_tlv_encode(p_buffer, p_buffer_len, &tlv);
}


uint32_t lwm2m_tlv_integer_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, int32_t value)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);

    uint8_t value_buffer[8];
    uint8_t value_length;

    lwm2m_tlv_int32_to_bytebuffer(value_buffer, &value_length, value);

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_VAL,
        .id      = id,
        .length  = value_length,
        .value   = value_buffer
    };

    return lwm2m_tlv_encode(p_buffer, p_buffer_len, &tlv);
}


uint32_t lwm2m_tlv_bool_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, bool value)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);

    uint8_t value_buffer[1];

    if (value == true)
    {
        value_buffer[0] = 1;
    }
    else
    {
        value_buffer[0] = 0;
    }

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_VAL,
        .id      = id,
        .length  = 1,
        .value   = value_buffer
    };

    return lwm2m_tlv_encode(p_buffer, p_buffer_len, &tlv);
}


uint32_t lwm2m_tlv_opaque_encode(uint8_t * p_buffer, uint32_t * p_buffer_len, uint16_t id, lwm2m_opaque_t value)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);

    lwm2m_tlv_t tlv =
    {
        .id_type = TLV_TYPE_RESOURCE_VAL,
        .id      = id,
        .length  = value.len,
        .value   = value.p_val
    };

    return lwm2m_tlv_encode(p_buffer, p_buffer_len, &tlv);
}
