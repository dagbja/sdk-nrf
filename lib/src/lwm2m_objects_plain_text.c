/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>

#include <lwm2m_objects_plain_text.h>
#include <lwm2m_objects.h>


static uint32_t lwm2m_plain_text_to_int32(uint8_t * p_payload, uint16_t payload_len, int32_t * p_value)
{
    NULL_PARAM_CHECK(p_payload);
    NULL_PARAM_CHECK(p_value);

    char   payload[payload_len + 1];
    char * p_end = NULL;

    // Make a copy of the payload to add null termination.
    memcpy(payload, p_payload, payload_len);
    payload[payload_len] = '\0';

    int32_t value = (int32_t)strtol(payload, &p_end, 10);

    if (p_end != (payload + payload_len))
    {
        return ENOENT;
    }

    *p_value = value;

    return 0;
}

uint32_t lwm2m_plain_text_server_decode(lwm2m_server_t * p_server,
                                        uint16_t         resource_id,
                                        uint8_t        * p_buffer,
                                        uint32_t         buffer_len)
{
    NULL_PARAM_CHECK(p_server);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t err_code;
    int32_t  value;

    switch (resource_id)
    {
        case LWM2M_SERVER_SHORT_SERVER_ID:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &value);

            if (err_code == 0)
            {
                if ((value >= 0) && (value <= UINT16_MAX))
                {
                    p_server->short_server_id = (uint16_t)value;
                }
                else
                {
                    err_code = EINVAL;
                }
            }
            break;
        }

        case LWM2M_SERVER_LIFETIME:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &p_server->lifetime);
            break;
        }

        case LWM2M_SERVER_DEFAULT_MIN_PERIOD:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &p_server->default_minimum_period);
            break;
        }

        case LWM2M_SERVER_DEFAULT_MAX_PERIOD:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &p_server->default_maximum_period);
            break;
        }

        case LWM2M_SERVER_DISABLE_TIMEOUT:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &p_server->disable_timeout);
            break;
        }

        case LWM2M_SERVER_NOTIFY_WHEN_DISABLED:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &value);

            if (err_code == 0)
            {
                if ((value >= 0) && (value <= UINT8_MAX))
                {
                    p_server->notification_storing_on_disabled = (uint8_t)value;
                }
                else
                {
                    err_code = EINVAL;
                }
            }
            break;
        }

        case LWM2M_SERVER_BINDING:
        {
            err_code = lwm2m_bytebuffer_to_string((char *)p_buffer,
                                                  buffer_len,
                                                  &p_server->binding);
            break;
        }

        default:
            err_code = ENOTSUP;
            break;
    }

    return err_code;
}


uint32_t lwm2m_plain_text_device_decode(lwm2m_device_t * p_device,
                                        uint16_t         resource_id,
                                        uint8_t        * p_buffer,
                                        uint32_t         buffer_len)
{
    NULL_PARAM_CHECK(p_device);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_DEVICE_CURRENT_TIME:
        {
            err_code = lwm2m_plain_text_to_int32(p_buffer,
                                                 buffer_len,
                                                 &p_device->current_time);
            break;
        }

        case LWM2M_DEVICE_UTC_OFFSET:
        {
            err_code = lwm2m_bytebuffer_to_string((char *)p_buffer,
                                                  buffer_len,
                                                  &p_device->utc_offset);
            break;
        }

        case LWM2M_DEVICE_TIMEZONE:
        {
            err_code = lwm2m_bytebuffer_to_string((char *)p_buffer,
                                                  buffer_len,
                                                  &p_device->timezone);
            break;
        }

        default:
            err_code = ENOTSUP;
            break;
    }

    return err_code;
}
