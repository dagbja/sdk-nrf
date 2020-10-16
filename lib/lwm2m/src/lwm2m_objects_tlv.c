/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects_tlv.h>


static void index_buffer_len_update(uint32_t * index, uint32_t * buffer_len, uint32_t max_buffer)
{
    *index     += *buffer_len;
    *buffer_len = max_buffer - *index;
}

static uint32_t lwm2m_tlv_resource_encode(uint8_t          * p_buffer,
                                          uint32_t         * p_buffer_len,
                                          lwm2m_instance_t * p_instance,
                                          uint16_t           resource_id)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_instance);

    uint32_t err_code = 0;

    switch (p_instance->object_id)
    {
        case LWM2M_OBJ_SECURITY:
        {
            err_code = lwm2m_tlv_security_encode(p_buffer,
                                                 p_buffer_len,
                                                 resource_id,
                                                 (lwm2m_security_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_SERVER:
        {
            err_code = lwm2m_tlv_server_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               (lwm2m_server_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_ACCESS_CONTROL:
        {
            err_code = lwm2m_tlv_access_control_encode(p_buffer,
                                                       p_buffer_len,
                                                       resource_id,
                                                       (lwm2m_access_control_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_DEVICE:
        {
            err_code = lwm2m_tlv_device_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               (lwm2m_device_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_CONN_MON:
        {
            err_code = lwm2m_tlv_connectivity_monitoring_encode(p_buffer,
                                                                p_buffer_len,
                                                                resource_id,
                                                                (lwm2m_connectivity_monitoring_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_FIRMWARE:
            err_code = lwm2m_tlv_firmware_encode(p_buffer,
                                                 p_buffer_len,
                                                 resource_id,
                                                 (lwm2m_firmware_t *)p_instance);
            break;

        case LWM2M_OBJ_CONN_STAT:
        {
            err_code = lwm2m_tlv_connectivity_statistics_encode(p_buffer,
                                                                p_buffer_len,
                                                                resource_id,
                                                                (lwm2m_connectivity_statistics_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_APN_CONNECTION_PROFILE:
        {
            err_code = lwm2m_tlv_apn_connection_profile_encode(p_buffer,
                                                              p_buffer_len,
                                                              resource_id,
                                                              (lwm2m_apn_conn_prof_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_PORTFOLIO:
        {
            err_code = lwm2m_tlv_portfolio_encode(p_buffer,
                                                  p_buffer_len,
                                                  resource_id,
                                                  (lwm2m_portfolio_t *)p_instance);
            break;
        }

        case LWM2M_OBJ_CONN_EXT:
        {
            err_code = lwm2m_tlv_connectivity_extension_encode(p_buffer,
                                                               p_buffer_len,
                                                               resource_id,
                                                               (lwm2m_connectivity_extension_t *)p_instance);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_instance_encode(uint8_t          * p_buffer,
                                   uint32_t         * p_buffer_len,
                                   lwm2m_instance_t * p_instance,
                                   bool               check_permissions)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_instance);

    uint32_t err_code;
    uint32_t max_buffer = *p_buffer_len;
    uint32_t index      = 0;

    uint8_t  * p_operations   = (uint8_t *)p_instance + p_instance->operations_offset;
    uint16_t * p_resource_ids = (uint16_t *)((uint8_t *)p_instance + p_instance->resource_ids_offset);

    for (uint32_t i = 0; i < p_instance->num_resources; i++)
    {
        if (check_permissions &&
            !(p_operations[i] & LWM2M_OPERATION_CODE_READ))
        {
            continue;
        }

        err_code = lwm2m_tlv_resource_encode(&p_buffer[index], p_buffer_len, p_instance, p_resource_ids[i]);

        if (err_code != 0)
        {
            return err_code;
        }

        index_buffer_len_update(&index, p_buffer_len, max_buffer);
    }

    *p_buffer_len = index;

    return 0;
}


static uint32_t lwm2m_tlv_object_encode(uint8_t          * p_buffer,
                                        uint32_t         * p_buffer_len,
                                        lwm2m_object_t   * p_object)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_object);

    uint32_t err_code            = 0;
    uint32_t instance_buffer_len = 256;
    uint8_t  instance_buffer[instance_buffer_len];
    uint32_t current_len         = *p_buffer_len;
    uint32_t max_buffer          = *p_buffer_len;
    uint32_t index               = 0;
    uint16_t object_id           = p_object->object_id;
    lwm2m_instance_t *p_instance = NULL;

    for (int i = 0; i < LWM2M_COAP_HANDLER_MAX_INSTANCES; i++)
    {
        err_code = lwm2m_lookup_instance(&p_instance, object_id, i);

        if (err_code != 0)
        {
            err_code = 0;
            continue;
        }

        instance_buffer_len = sizeof(instance_buffer);

        err_code = lwm2m_tlv_instance_encode(instance_buffer, &instance_buffer_len, p_instance, true);

        if (err_code != 0)
        {
            break;
        }

        lwm2m_tlv_t tlv = {
            .id_type = TLV_TYPE_OBJECT,
            .id = i,
            .length = instance_buffer_len
        };

        err_code = lwm2m_tlv_header_encode(&p_buffer[index], &current_len, &tlv);

        if (err_code != 0)
        {
            break;
        }

        index += current_len;

        if ((index < max_buffer) && ((max_buffer - index) > instance_buffer_len))
        {
            memcpy(&p_buffer[index], instance_buffer, instance_buffer_len);
            index += instance_buffer_len;
            current_len = max_buffer - index;
        }
        else
        {
            err_code = ENOMEM;
            break;
        }
    }

    *p_buffer_len = index;

    return err_code;
}


uint32_t lwm2m_tlv_element_encode(uint8_t        * p_buffer,
                                  uint32_t       * p_buffer_len,
                                  const uint16_t * p_path,
                                  uint8_t          path_len)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_path);

    uint32_t err_code;

    if (path_len == 1)
    {
        lwm2m_object_t *p_object;

        err_code = lwm2m_lookup_object(&p_object, p_path[0]);

        if (err_code != 0)
        {
            return err_code;
        }

        err_code = lwm2m_tlv_object_encode(p_buffer, p_buffer_len, p_object);
    }
    else if (path_len == 2)
    {
        lwm2m_instance_t *p_instance;

        err_code = lwm2m_lookup_instance(&p_instance, p_path[0], p_path[1]);

        if (err_code != 0)
        {
            return err_code;
        }

        err_code = lwm2m_tlv_instance_encode(p_buffer, p_buffer_len, p_instance, true);
    }
    else if (path_len == 3)
    {
        lwm2m_instance_t *p_instance;

        err_code = lwm2m_lookup_instance(&p_instance, p_path[0], p_path[1]);

        if (err_code != 0)
        {
            return err_code;
        }

        err_code = lwm2m_tlv_resource_encode(p_buffer, p_buffer_len, p_instance, p_path[2]);
    }
    else
    {
        LWM2M_WRN("Unsupported URI path length %d", path_len);
        err_code = EINVAL;
    }

    return err_code;
}


uint32_t lwm2m_tlv_security_decode(lwm2m_security_t   * p_security,
                                   uint8_t            * p_buffer,
                                   uint32_t             buffer_len,
                                   lwm2m_tlv_callback_t resource_callback)
{
    NULL_PARAM_CHECK(p_security);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t    err_code;
    uint32_t    index = 0;
    lwm2m_tlv_t tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_SECURITY_SERVER_URI:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_security->server_uri);
                break;
            }

            case LWM2M_SECURITY_BOOTSTRAP_SERVER:
            {
                p_security->bootstrap_server = tlv.value[0];
                break;
            }

            case LWM2M_SECURITY_SECURITY_MODE:
            {
                p_security->security_mode = tlv.value[0];
                break;
            }

            case LWM2M_SECURITY_PUBLIC_KEY:
            {
                err_code = lwm2m_bytebuffer_to_opaque(tlv.value,
                                                      tlv.length,
                                                      &p_security->public_key);
                break;
            }

            case LWM2M_SECURITY_SERVER_PUBLIC_KEY:
            {
                err_code = lwm2m_bytebuffer_to_opaque(tlv.value,
                                                      tlv.length,
                                                      &p_security->server_public_key);
                break;
            }

            case LWM2M_SECURITY_SECRET_KEY:
            {
                err_code = lwm2m_bytebuffer_to_opaque(tlv.value,
                                                      tlv.length,
                                                      &p_security->secret_key);
                break;
            }

            case LWM2M_SECURITY_SMS_SECURITY_MODE:
            {
                p_security->sms_security_mode = tlv.value[0];
                break;
            }

            case LWM2M_SECURITY_SMS_BINDING_KEY_PARAM:
            {
                err_code = lwm2m_bytebuffer_to_opaque(tlv.value,
                                                      tlv.length,
                                                      &p_security->sms_binding_key_param);
                break;
            }

            case LWM2M_SECURITY_SMS_BINDING_SECRET_KEY:
            {
                err_code = lwm2m_bytebuffer_to_opaque(tlv.value,
                                                      tlv.length,
                                                      &p_security->sms_binding_secret_keys);
                break;
            }

            case LWM2M_SECURITY_SERVER_SMS_NUMBER:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_security->sms_number);
                break;
            }

            case LWM2M_SECURITY_SHORT_SERVER_ID:
            {
                err_code = lwm2m_tlv_bytebuffer_to_uint16(tlv.value,
                                                          tlv.length,
                                                          &p_security->short_server_id);
                break;
            }

            case LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_security->client_hold_off_time);
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(p_security->proto.instance_id, &tlv);
                }
                else
                {
                        LWM2M_WRN("Unhandled resource /0/%d/%d when decoding",
                                  p_security->proto.instance_id, tlv.id);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_security_encode(uint8_t          * p_buffer,
                                   uint32_t         * p_buffer_len,
                                   uint16_t           resource_id,
                                   lwm2m_security_t * p_security)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_security);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_SECURITY_SERVER_URI:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_SERVER_URI,
                                               p_security->server_uri);
            break;
        }

        case LWM2M_SECURITY_BOOTSTRAP_SERVER:
        {
            err_code = lwm2m_tlv_bool_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_SECURITY_BOOTSTRAP_SERVER,
                                             p_security->bootstrap_server);
            break;
        }

        case LWM2M_SECURITY_SECURITY_MODE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SECURITY_SECURITY_MODE,
                                                p_security->security_mode);
            break;
        }

        case LWM2M_SECURITY_PUBLIC_KEY:
        {
            err_code = lwm2m_tlv_opaque_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_PUBLIC_KEY,
                                               p_security->public_key);
            break;
        }

        case LWM2M_SECURITY_SERVER_PUBLIC_KEY:
        {
            err_code = lwm2m_tlv_opaque_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_SERVER_PUBLIC_KEY,
                                               p_security->server_public_key);
            break;
        }

        case LWM2M_SECURITY_SECRET_KEY:
        {
            // Omit the encoding of this specific resource to make sure
            // it is not written to flash when saving this instance.
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }

        case LWM2M_SECURITY_SMS_SECURITY_MODE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SECURITY_SMS_SECURITY_MODE,
                                                p_security->sms_security_mode);
            break;
        }

        case LWM2M_SECURITY_SMS_BINDING_KEY_PARAM:
        {
            err_code = lwm2m_tlv_opaque_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_SMS_BINDING_KEY_PARAM,
                                               p_security->sms_binding_key_param);
            break;
        }

        case LWM2M_SECURITY_SMS_BINDING_SECRET_KEY:
        {
            err_code = lwm2m_tlv_opaque_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_SMS_BINDING_SECRET_KEY,
                                               p_security->sms_binding_secret_keys);
            break;
        }

        case LWM2M_SECURITY_SERVER_SMS_NUMBER:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SECURITY_SERVER_SMS_NUMBER,
                                               p_security->sms_number);
            break;
        }

        case LWM2M_SECURITY_SHORT_SERVER_ID:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SECURITY_SHORT_SERVER_ID,
                                                p_security->short_server_id);
            break;
        }

        case LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME,
                                                p_security->client_hold_off_time);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_security,
                                                 true);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_server_decode(lwm2m_server_t     * p_server,
                                 uint8_t            * p_buffer,
                                 uint32_t             buffer_len,
                                 lwm2m_tlv_callback_t resource_callback)
{
    NULL_PARAM_CHECK(p_server);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t    err_code;
    uint32_t    index = 0;
    lwm2m_tlv_t tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);
        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_SERVER_SHORT_SERVER_ID:
            {
                err_code = lwm2m_tlv_bytebuffer_to_uint16(tlv.value,
                                                          tlv.length,
                                                          &p_server->short_server_id);
                break;
            }

            case LWM2M_SERVER_LIFETIME:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_server->lifetime);
                break;
            }

            case LWM2M_SERVER_DEFAULT_MIN_PERIOD:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_server->default_minimum_period);
                break;
            }

            case LWM2M_SERVER_DEFAULT_MAX_PERIOD:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_server->default_maximum_period);
                break;
            }

            case LWM2M_SERVER_DISABLE:
            {
                // Execute do nothing
                break;
            }

            case LWM2M_SERVER_DISABLE_TIMEOUT:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_server->disable_timeout);
                break;
            }

            case LWM2M_SERVER_NOTIFY_WHEN_DISABLED:
            {
                p_server->notification_storing_on_disabled = tlv.value[0];
                break;
            }

            case LWM2M_SERVER_BINDING:
            {
                err_code = lwm2m_bytebuffer_to_string((char *)tlv.value,
                                                      tlv.length,
                                                      &p_server->binding);
                break;
            }

            case LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER:
            {
                // Execute do nothing
                break;
            }

            case LWM2M_SERVER_BOOTSTRAP_REQUEST_TRIGGER:
            {
                // Execute do nothing
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(p_server->proto.instance_id, &tlv);
                }
                else
                {
                        LWM2M_WRN("Unhandled resource /1/%d/%d when decoding",
                                  p_server->proto.instance_id, tlv.id);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_server_encode(uint8_t        * p_buffer,
                                 uint32_t       * p_buffer_len,
                                 uint16_t         resource_id,
                                 lwm2m_server_t * p_server)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_server);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_SERVER_SHORT_SERVER_ID:
        {
            // Encode short server id.
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SERVER_SHORT_SERVER_ID,
                                                p_server->short_server_id);
            break;
        }

        case LWM2M_SERVER_LIFETIME:
        {
            // Encode lifetime.
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SERVER_LIFETIME,
                                                p_server->lifetime);
            break;
        }

        case LWM2M_SERVER_DEFAULT_MIN_PERIOD:
        {
            // Encode default minimum period.
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SERVER_DEFAULT_MIN_PERIOD,
                                                p_server->default_minimum_period);
            break;
        }

        case LWM2M_SERVER_DEFAULT_MAX_PERIOD:
        {
            // Encode default maximum period.
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SERVER_DEFAULT_MAX_PERIOD,
                                                p_server->default_maximum_period);
            break;
        }

        case LWM2M_SERVER_DISABLE_TIMEOUT:
        {
            // Encode disable timeout.
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_SERVER_DISABLE_TIMEOUT,
                                                p_server->disable_timeout);
            break;
        }

        case LWM2M_SERVER_NOTIFY_WHEN_DISABLED:
        {
            // Encode Notify when disabled.
            err_code = lwm2m_tlv_bool_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_SERVER_NOTIFY_WHEN_DISABLED,
                                             p_server->notification_storing_on_disabled);
            break;
        }

        case LWM2M_SERVER_BINDING:
        {
            // Encode binding.
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_SERVER_BINDING,
                                               p_server->binding);
            break;
        }

        // These resource are normally not encoded since they are executable,
        // unless the permission check is omitted, which happens when objects
        // are being saved to flash. In that case we must take care of updating
        // p_buffer_len or the caller will think the buffer was filled.
        case LWM2M_SERVER_DISABLE:
        case LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER:
        case LWM2M_SERVER_BOOTSTRAP_REQUEST_TRIGGER:
        {
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_server,
                                                 true);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_access_control_decode(lwm2m_access_control_t * p_access_control,
                                         uint8_t                * p_buffer,
                                         uint32_t                 buffer_len,
                                         lwm2m_tlv_callback_t     resource_callback)
{
    NULL_PARAM_CHECK(p_access_control);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t    err_code;
    uint32_t    index = 0;
    lwm2m_tlv_t tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_ACCESS_CONTROL_OBJECT_ID:
            {
                err_code = lwm2m_tlv_bytebuffer_to_uint16(tlv.value,
                                                          tlv.length,
                                                          &p_access_control->object_id);
                break;
            }

            case LWM2M_ACCESS_CONTROL_INSTANCE_ID:
            {
                err_code = lwm2m_tlv_bytebuffer_to_uint16(tlv.value,
                                                          tlv.length,
                                                          &p_access_control->instance_id);
                break;
            }

            case LWM2M_ACCESS_CONTROL_ACL:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_access_control->acl);
                break;
            }

            case LWM2M_ACCESS_CONTROL_CONTROL_OWNER:
            {
                err_code = lwm2m_tlv_bytebuffer_to_uint16(tlv.value,
                                                          tlv.length,
                                                          &p_access_control->control_owner);
                break;
            }

            default:
            {
                err_code = 0;
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        if (resource_callback)
        {
            err_code = resource_callback(p_access_control->proto.instance_id, &tlv);
            if (err_code != 0)
            {
                return err_code;
            }
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_access_control_encode(uint8_t                * p_buffer,
                                         uint32_t               * p_buffer_len,
                                         uint16_t                 resource_id,
                                         lwm2m_access_control_t * p_access_control)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_access_control);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_ACCESS_CONTROL_OBJECT_ID:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_ACCESS_CONTROL_OBJECT_ID,
                                                p_access_control->object_id);
            break;
        }

        case LWM2M_ACCESS_CONTROL_INSTANCE_ID:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_ACCESS_CONTROL_INSTANCE_ID,
                                                p_access_control->instance_id);
            break;
        }

        case LWM2M_ACCESS_CONTROL_ACL:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_ACCESS_CONTROL_ACL,
                                             &p_access_control->acl);
            break;
        }

        case LWM2M_ACCESS_CONTROL_CONTROL_OWNER:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_ACCESS_CONTROL_CONTROL_OWNER,
                                                p_access_control->control_owner);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_access_control,
                                                 true);
            break;
        }


        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_connectivity_monitoring_decode(lwm2m_connectivity_monitoring_t * p_conn_mon,
                                                  uint8_t                         * p_buffer,
                                                  uint32_t                          buffer_len,
                                                  lwm2m_tlv_callback_t              resource_callback)
{
    NULL_PARAM_CHECK(p_conn_mon);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_CONN_MON_NETWORK_BEARER:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->network_bearer);
                break;
            }

            case LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_mon->available_network_bearer);
                break;
            }

            case LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->radio_signal_strength);
                break;
            }

            case LWM2M_CONN_MON_LINK_QUALITY:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->link_quality);
                break;
            }

            case LWM2M_CONN_MON_IP_ADDRESSES:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_mon->ip_addresses);
                break;
            }

            case LWM2M_CONN_MON_ROUTER_IP_ADRESSES:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_mon->router_ip_addresses);
                break;
            }

            case LWM2M_CONN_MON_LINK_UTILIZATION:
            {
                p_conn_mon->link_utilization = tlv.value[0];
                break;
            }

            case LWM2M_CONN_MON_APN:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_mon->apn);
                break;
            }

            case LWM2M_CONN_MON_CELL_ID:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->cell_id);
                break;
            }

            case LWM2M_CONN_MON_SMNC:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->smnc);
                break;
            }

            case LWM2M_CONN_MON_SMCC:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_mon->smcc);
                break;
            }

            default:
            {
                err_code = 0;
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        if (resource_callback)
        {
            err_code = resource_callback(p_conn_mon->proto.instance_id, &tlv);
            if (err_code != 0)
            {
                return err_code;
            }
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_connectivity_monitoring_encode(uint8_t                         * p_buffer,
                                                  uint32_t                        * p_buffer_len,
                                                  uint16_t                          resource_id,
                                                  lwm2m_connectivity_monitoring_t * p_conn_mon)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_conn_mon);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_CONN_MON_NETWORK_BEARER:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_NETWORK_BEARER,
                                                p_conn_mon->network_bearer);
            break;
        }

        case LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER,
                                             &p_conn_mon->available_network_bearer);
            break;
        }

        case LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH,
                                                p_conn_mon->radio_signal_strength);
            break;
        }

        case LWM2M_CONN_MON_LINK_QUALITY:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_LINK_QUALITY,
                                                p_conn_mon->link_quality);
            break;
        }

        case LWM2M_CONN_MON_IP_ADDRESSES:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_MON_IP_ADDRESSES,
                                             &p_conn_mon->ip_addresses);
            break;
        }

        case LWM2M_CONN_MON_ROUTER_IP_ADRESSES:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_MON_ROUTER_IP_ADRESSES,
                                             &p_conn_mon->router_ip_addresses);
            break;
        }

        case LWM2M_CONN_MON_LINK_UTILIZATION:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_LINK_UTILIZATION,
                                                p_conn_mon->link_utilization);
            break;
        }

        case LWM2M_CONN_MON_APN:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_MON_APN,
                                             &p_conn_mon->apn);
            break;
        }

        case LWM2M_CONN_MON_CELL_ID:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_CELL_ID,
                                                p_conn_mon->cell_id);
            break;
        }

        case LWM2M_CONN_MON_SMNC:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_SMNC,
                                                p_conn_mon->smnc);
            break;
        }

        case LWM2M_CONN_MON_SMCC:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_MON_SMCC,
                                                p_conn_mon->smcc);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_conn_mon,
                                                 true);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_device_decode(lwm2m_device_t     * p_device,
                                 uint8_t            * p_buffer,
                                 uint32_t             buffer_len,
                                 lwm2m_tlv_callback_t resource_callback)
{
    NULL_PARAM_CHECK(p_device);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_DEVICE_CURRENT_TIME:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_device->current_time);
                break;
            }

            case LWM2M_DEVICE_UTC_OFFSET:
            {
                err_code = lwm2m_bytebuffer_to_string((char *)tlv.value,
                                                      tlv.length,
                                                      &p_device->utc_offset);
                break;
            }

            case LWM2M_DEVICE_TIMEZONE:
            {
                err_code = lwm2m_bytebuffer_to_string((char *)tlv.value,
                                                      tlv.length,
                                                      &p_device->timezone);
                break;
            }

            default:
            {
                err_code = 0;
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        if (resource_callback)
        {
            err_code = resource_callback(p_device->proto.instance_id, &tlv);
            if (err_code != 0)
            {
                return err_code;
            }
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_device_encode(uint8_t        * p_buffer,
                                 uint32_t       * p_buffer_len,
                                 uint16_t         resource_id,
                                 lwm2m_device_t * p_device)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_device);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_DEVICE_MANUFACTURER:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_MANUFACTURER,
                                               p_device->manufacturer);
            break;
        }

        case LWM2M_DEVICE_MODEL_NUMBER:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_MODEL_NUMBER,
                                               p_device->model_number);
            break;
        }

        case LWM2M_DEVICE_SERIAL_NUMBER:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_SERIAL_NUMBER,
                                               p_device->serial_number);
            break;
        }

        case LWM2M_DEVICE_FIRMWARE_VERSION:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_FIRMWARE_VERSION,
                                               p_device->firmware_version);
            break;
        }

        case LWM2M_DEVICE_AVAILABLE_POWER_SOURCES:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_DEVICE_AVAILABLE_POWER_SOURCES,
                                             &p_device->avail_power_sources);
            break;
        }

        case LWM2M_DEVICE_POWER_SOURCE_VOLTAGE:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_DEVICE_POWER_SOURCE_VOLTAGE,
                                             &p_device->power_source_voltage);
            break;
        }

        case LWM2M_DEVICE_POWER_SOURCE_CURRENT:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_DEVICE_POWER_SOURCE_CURRENT,
                                             &p_device->power_source_current);
            break;
        }

        case LWM2M_DEVICE_BATTERY_LEVEL:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_DEVICE_BATTERY_LEVEL,
                                                p_device->battery_level);
            break;
        }

        case LWM2M_DEVICE_MEMORY_FREE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_DEVICE_MEMORY_FREE,
                                                p_device->memory_free);
            break;
        }

        case LWM2M_DEVICE_ERROR_CODE:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_DEVICE_ERROR_CODE,
                                             &p_device->error_code);
            break;
        }

        case LWM2M_DEVICE_CURRENT_TIME:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_DEVICE_CURRENT_TIME,
                                                p_device->current_time);
            break;
        }

        case LWM2M_DEVICE_UTC_OFFSET:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_UTC_OFFSET,
                                               p_device->utc_offset);
            break;
        }

        case LWM2M_DEVICE_TIMEZONE:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_TIMEZONE,
                                               p_device->timezone);
            break;
        }

        case LWM2M_DEVICE_SUPPORTED_BINDINGS:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_SUPPORTED_BINDINGS,
                                               p_device->supported_bindings);
            break;
        }

        case LWM2M_DEVICE_DEVICE_TYPE:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_DEVICE_TYPE,
                                               p_device->device_type);
            break;
        }

        case LWM2M_DEVICE_HARDWARE_VERSION:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_HARDWARE_VERSION,
                                               p_device->hardware_version);
            break;
        }

        case LWM2M_DEVICE_SOFTWARE_VERSION:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_DEVICE_SOFTWARE_VERSION,
                                               p_device->software_version);
            break;
        }

        case LWM2M_DEVICE_BATTERY_STATUS:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_DEVICE_BATTERY_STATUS,
                                                p_device->battery_status);
            break;
        }

        case LWM2M_DEVICE_MEMORY_TOTAL:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_DEVICE_MEMORY_TOTAL,
                                                p_device->memory_total);
            break;
        }

        case LWM2M_DEVICE_EXT_DEV_INFO:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_DEVICE_EXT_DEV_INFO,
                                             &p_device->ext_dev_info);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_device,
                                                 true);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}


uint32_t lwm2m_tlv_firmware_decode(lwm2m_firmware_t     * p_firmware,
                                   uint8_t              * p_buffer,
                                   uint32_t               buffer_len,
                                   lwm2m_tlv_callback_t   resource_callback)
{
    NULL_PARAM_CHECK(p_firmware);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);
        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_FIRMWARE_PACKAGE_URI:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_firmware->package_uri);
                break;
            }

            default:
            {
                err_code = 0;
                break;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        if (resource_callback)
        {
            err_code = resource_callback(p_firmware->proto.instance_id, &tlv);
            if (err_code != 0)
            {
                return err_code;
            }
        }
    }

    return 0;
}

uint32_t lwm2m_tlv_firmware_encode(uint8_t          * p_buffer,
                                   uint32_t         * p_buffer_len,
                                   uint16_t           resource_id,
                                   lwm2m_firmware_t * p_firmware)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_firmware);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_FIRMWARE_PACKAGE_URI:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_FIRMWARE_PACKAGE_URI,
                                               p_firmware->package_uri);
            break;
        }

        case LWM2M_FIRMWARE_STATE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_FIRMWARE_STATE,
                                                p_firmware->state);
            break;
        }

        case LWM2M_FIRMWARE_UPDATE_RESULT:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_FIRMWARE_UPDATE_RESULT,
                                                p_firmware->update_result);
            break;
        }

        case LWM2M_FIRMWARE_PKG_NAME:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_FIRMWARE_PKG_NAME,
                                               p_firmware->pkg_name);
            break;
        }

        case LWM2M_FIRMWARE_PKG_VERSION:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               LWM2M_FIRMWARE_PKG_VERSION,
                                               p_firmware->pkg_version);
            break;
        }

        case LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT,
                                             &p_firmware->firmware_update_protocol_support);
            break;
        }

        case LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD,
                                                p_firmware->firmware_update_delivery_method);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_firmware,
                                                 true);
            break;
        }

        default:
        {
            err_code = ENOTSUP;
            break;
        }
    }

    return err_code;
}

uint32_t lwm2m_tlv_connectivity_statistics_decode(lwm2m_connectivity_statistics_t * p_conn_stat,
                                                  uint8_t                         * p_buffer,
                                                  uint32_t                          buffer_len,
                                                  lwm2m_tlv_callback_t              resource_callback)
{
    NULL_PARAM_CHECK(p_conn_stat);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_CONN_STAT_COLLECTION_PERIOD:
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &p_conn_stat->collection_period);
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(p_conn_stat->proto.instance_id, &tlv);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return EINVAL;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_connectivity_statistics_encode(uint8_t                         * p_buffer,
                                                  uint32_t                        * p_buffer_len,
                                                  uint16_t                          resource_id,
                                                  lwm2m_connectivity_statistics_t * p_conn_stat)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_conn_stat);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_CONN_STAT_SMS_TX_COUNTER:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->sms_tx_counter);
            break;
        }

        case LWM2M_CONN_STAT_SMS_RX_COUNTER:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->sms_rx_counter);
            break;
        }

        case LWM2M_CONN_STAT_TX_DATA:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->tx_data);
            break;
        }

        case LWM2M_CONN_STAT_RX_DATA:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->rx_data);
            break;
        }

        case LWM2M_CONN_STAT_MAX_MSG_SIZE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->max_message_size);
            break;
        }

        case LWM2M_CONN_STAT_AVG_MSG_SIZE:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->average_message_size);
            break;
        }

        case LWM2M_CONN_STAT_COLLECTION_PERIOD:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                resource_id,
                                                p_conn_stat->collection_period);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_conn_stat,
                                                 true);
            break;
        }

        default:
        {
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }
    }

    return err_code;
}

uint32_t lwm2m_tlv_apn_connection_profile_decode(lwm2m_apn_conn_prof_t * p_apn_conn_prof,
                                                 uint8_t               * p_buffer,
                                                 uint32_t                buffer_len,
                                                 lwm2m_tlv_callback_t    resource_callback)
{
    NULL_PARAM_CHECK(p_apn_conn_prof);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_APN_CONN_PROF_PROFILE_NAME:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_apn_conn_prof->profile_name);
                break;
            }

            case LWM2M_APN_CONN_PROF_APN:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_apn_conn_prof->apn);
                break;
            }

            case LWM2M_APN_CONN_PROF_ENABLE_STATUS:
            {
                p_apn_conn_prof->enable_status = tlv.value[0];
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(((lwm2m_instance_t *)p_apn_conn_prof)->instance_id, &tlv);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return EINVAL;
        }
    }

    return 0;
}

uint32_t lwm2m_tlv_apn_connection_profile_encode(uint8_t               * p_buffer,
                                                 uint32_t              * p_buffer_len,
                                                 uint16_t                resource_id,
                                                 lwm2m_apn_conn_prof_t * p_apn_conn_prof)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_apn_conn_prof);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_APN_CONN_PROF_PROFILE_NAME:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_apn_conn_prof->profile_name);
            break;
        }

        case LWM2M_APN_CONN_PROF_APN:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_apn_conn_prof->apn);
            break;
        }

        case LWM2M_APN_CONN_PROF_ENABLE_STATUS:
        {
            err_code = lwm2m_tlv_bool_encode(p_buffer,
                                             p_buffer_len,
                                             resource_id,
                                             p_apn_conn_prof->enable_status);
            break;
        }

        case LWM2M_APN_CONN_PROF_CONN_EST_TIME:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_APN_CONN_PROF_CONN_EST_TIME,
                                             &p_apn_conn_prof->conn_est_time);
            break;
        }

        case LWM2M_APN_CONN_PROF_CONN_EST_RESULT:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_APN_CONN_PROF_CONN_EST_RESULT,
                                             &p_apn_conn_prof->conn_est_result);
            break;
        }

        case LWM2M_APN_CONN_PROF_CONN_EST_REJECT_CAUSE:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_APN_CONN_PROF_CONN_EST_REJECT_CAUSE,
                                             &p_apn_conn_prof->conn_est_reject_cause);
            break;
        }

        case LWM2M_APN_CONN_PROF_CONN_END_TIME:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_APN_CONN_PROF_CONN_END_TIME,
                                             &p_apn_conn_prof->conn_end_time);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_apn_conn_prof,
                                                 true);
            break;
        }

        default:
        {
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }
    }

    return err_code;
}

uint32_t lwm2m_tlv_portfolio_decode(lwm2m_portfolio_t     * p_portfolio,
                                    uint8_t               * p_buffer,
                                    uint32_t                buffer_len,
                                    lwm2m_tlv_callback_t    resource_callback)
{
    NULL_PARAM_CHECK(p_portfolio);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_PORTFOLIO_IDENTITY:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_portfolio->identity);
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(((lwm2m_instance_t *)p_portfolio)->instance_id, &tlv);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return EINVAL;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_portfolio_encode(uint8_t           * p_buffer,
                                    uint32_t          * p_buffer_len,
                                    uint16_t            resource_id,
                                    lwm2m_portfolio_t * p_portfolio)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_portfolio);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_PORTFOLIO_IDENTITY:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             resource_id,
                                             &p_portfolio->identity);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_portfolio,
                                                 true);
            break;
        }

        default:
        {
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }
    }

    return err_code;
}

uint32_t lwm2m_tlv_connectivity_extension_decode(lwm2m_connectivity_extension_t * p_conn_ext,
                                                 uint8_t                        * p_buffer,
                                                 uint32_t                         buffer_len,
                                                 lwm2m_tlv_callback_t             resource_callback)
{
    NULL_PARAM_CHECK(p_conn_ext);
    NULL_PARAM_CHECK(p_buffer);

    uint32_t     err_code;
    uint32_t     index = 0;
    lwm2m_tlv_t  tlv;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case LWM2M_CONN_EXT_MSISDN:
            {
                err_code = lwm2m_bytebuffer_to_string(tlv.value,
                                                      tlv.length,
                                                      &p_conn_ext->msisdn);
                break;
            }

            case LWM2M_CONN_EXT_APN_RETRIES:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_ext->apn_retries);
                break;
            }

            case LWM2M_CONN_EXT_APN_RETRY_PERIOD:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_ext->apn_retry_period);
                break;
            }

            case LWM2M_CONN_EXT_APN_RETRY_BACK_OFF_PERIOD:
            {
                err_code = lwm2m_tlv_list_decode(tlv, &p_conn_ext->apn_retry_back_off_period);
                break;
            }

            default:
            {
                if (resource_callback)
                {
                    err_code = resource_callback(((lwm2m_instance_t *)p_conn_ext)->instance_id, &tlv);
                }
                break;
            }
        }

        if (err_code != 0)
        {
            return EINVAL;
        }
    }

    return 0;
}


uint32_t lwm2m_tlv_connectivity_extension_encode(uint8_t                        * p_buffer,
                                                 uint32_t                       * p_buffer_len,
                                                 uint16_t                         resource_id,
                                                 lwm2m_connectivity_extension_t * p_conn_ext)
{
    NULL_PARAM_CHECK(p_buffer);
    NULL_PARAM_CHECK(p_buffer_len);
    NULL_PARAM_CHECK(p_conn_ext);

    uint32_t err_code;

    switch (resource_id)
    {
        case LWM2M_CONN_EXT_ICCID:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_conn_ext->iccid);
            break;
        }

        case LWM2M_CONN_EXT_IMSI:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_conn_ext->imsi);
            break;
        }

        case LWM2M_CONN_EXT_MSISDN:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_conn_ext->msisdn);
            break;
        }

        case LWM2M_CONN_EXT_APN_RETRIES:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_EXT_APN_RETRIES,
                                             &p_conn_ext->apn_retries);
            break;
        }

        case LWM2M_CONN_EXT_APN_RETRY_PERIOD:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_EXT_APN_RETRY_PERIOD,
                                             &p_conn_ext->apn_retry_period);
            break;
        }

        case LWM2M_CONN_EXT_APN_RETRY_BACK_OFF_PERIOD:
        {
            err_code = lwm2m_tlv_list_encode(p_buffer,
                                             p_buffer_len,
                                             LWM2M_CONN_EXT_APN_RETRY_BACK_OFF_PERIOD,
                                             &p_conn_ext->apn_retry_back_off_period);
            break;
        }

        case LWM2M_CONN_EXT_SINR:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_EXT_SINR,
                                                p_conn_ext->sinr);
            break;
        }

        case LWM2M_CONN_EXT_SRXLEV:
        {
            err_code = lwm2m_tlv_integer_encode(p_buffer,
                                                p_buffer_len,
                                                LWM2M_CONN_EXT_SRXLEV,
                                                p_conn_ext->srxlev);
            break;
        }

        case LWM2M_CONN_EXT_CE_MODE:
        {
            err_code = lwm2m_tlv_string_encode(p_buffer,
                                               p_buffer_len,
                                               resource_id,
                                               p_conn_ext->ce_mode);
            break;
        }

        case LWM2M_NAMED_OBJECT:
        {
            // This is a callback to the instance, not a specific resource.
            err_code = lwm2m_tlv_instance_encode(p_buffer,
                                                 p_buffer_len,
                                                 (lwm2m_instance_t *)p_conn_ext,
                                                 true);
            break;
        }

        default:
        {
            err_code = 0;
            *p_buffer_len = 0;
            break;
        }
    }

    return err_code;
}
