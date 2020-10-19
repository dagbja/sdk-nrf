/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_access_control.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_observer.h>
#include <lwm2m_remote.h>
#include <lwm2m_server.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_carrier_main.h>
#include <operator_check.h>
#include <lwm2m_carrier.h>
#include <lwm2m_factory_bootstrap.h>
#include <lwm2m_security.h>

#include <coap_option.h>
#include <coap_observe_api.h>
#include <coap_message.h>


extern void app_server_disable(uint16_t instance_id);

#define VERIZON_RESOURCE 30000

#define APP_MOTIVE_FIX_UPDATE_TRIGGER   1 // To adjust for MotiveBridge posting /1/0/8 instead of /1/1/8

static lwm2m_server_t                      m_instance_server[1+LWM2M_MAX_SERVERS];            /**< Server object instance to be filled by the bootstrap server. */
static lwm2m_object_t                      m_object_server;                                   /**< LWM2M server base object. */

// Verizon specific resources.
static vzw_server_settings_t vzw_server_settings[1+LWM2M_MAX_SERVERS];

uint32_t lwm2m_server_registered_get(uint16_t instance_id)
{
    return vzw_server_settings[instance_id].is_registered;
}

void lwm2m_server_registered_set(uint16_t instance_id, uint32_t value)
{
    vzw_server_settings[instance_id].is_registered = value;
}

uint32_t lwm2m_server_client_hold_off_timer_get(uint16_t instance_id)
{
    return vzw_server_settings[instance_id].client_hold_off_timer;
}

void lwm2m_server_client_hold_off_timer_set(uint16_t instance_id, uint32_t value)
{
    vzw_server_settings[instance_id].client_hold_off_timer = value;
}

// LWM2M core resources.
lwm2m_time_t lwm2m_server_lifetime_get(uint16_t instance_id)
{
    return m_instance_server[instance_id].lifetime;
}

void lwm2m_server_lifetime_set(uint16_t instance_id, lwm2m_time_t value)
{
    m_instance_server[instance_id].lifetime = value;
}

lwm2m_time_t lwm2m_server_min_period_get(uint16_t instance_id)
{
    return m_instance_server[instance_id].default_minimum_period;
}

void lwm2m_server_min_period_set(uint16_t instance_id, lwm2m_time_t value)
{
    m_instance_server[instance_id].default_minimum_period = value;
}

lwm2m_time_t lwm2m_server_max_period_get(uint16_t instance_id)
{
    return m_instance_server[instance_id].default_maximum_period;
}

void lwm2m_server_max_period_set(uint16_t instance_id, lwm2m_time_t value)
{
    m_instance_server[instance_id].default_maximum_period = value;
}

lwm2m_time_t lwm2m_server_disable_timeout_get(uint16_t instance_id)
{
    lwm2m_time_t disable_timeout = m_instance_server[instance_id].disable_timeout;

    if (disable_timeout == 0) {
        disable_timeout = 86400; // If disable timeout is not set, a default timeout value is 86400 (1 day).
    }

    return disable_timeout;
}

void lwm2m_server_disable_timeout_set(uint16_t instance_id, lwm2m_time_t value)
{
    m_instance_server[instance_id].disable_timeout = value;
}

bool lwm2m_server_notif_storing_get(uint16_t instance_id)
{
    return m_instance_server[instance_id].notification_storing_on_disabled;
}

void lwm2m_server_notif_storing_set(uint16_t instance_id, bool value)
{
    m_instance_server[instance_id].notification_storing_on_disabled = value;
}

char * lwm2m_server_binding_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_server[instance_id].binding.len;
    return m_instance_server[instance_id].binding.p_val;
}

void lwm2m_server_binding_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_string(p_value, len, &m_instance_server[instance_id].binding) != 0)
    {
        LWM2M_ERR("Could not set binding");
    }
}

uint16_t lwm2m_server_short_server_id_get(uint16_t instance_id)
{
    return m_instance_server[instance_id].short_server_id;
}

void lwm2m_server_short_server_id_set(uint16_t instance_id, uint16_t value)
{
    m_instance_server[instance_id].short_server_id = value;
}

lwm2m_server_t * lwm2m_server_get_instance(uint16_t instance_id)
{
    return &m_instance_server[instance_id];
}

lwm2m_object_t * lwm2m_server_get_object(void)
{
    return &m_object_server;
}

static uint32_t tlv_server_vzw_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    int32_t list_values[2] =
    {
        vzw_server_settings[instance_id].is_registered,
        vzw_server_settings[instance_id].client_hold_off_timer
    };

    lwm2m_list_t list =
    {
        .type        = LWM2M_LIST_TYPE_INT32,
        .p_id        = NULL,
        .val.p_int32 = list_values,
        .len         = ARRAY_SIZE(list_values)
    };

    return lwm2m_tlv_list_encode(
            p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}

uint32_t tlv_server_carrier_encode(uint16_t instance_id, uint8_t * p_buffer,
                                   uint32_t * p_buffer_len)
{
    if (!operator_is_vzw(true)) {
        /* Nothing to encode */
        *p_buffer_len = 0;
        return 0;
    }

    return tlv_server_vzw_encode(instance_id, p_buffer, p_buffer_len);
}

uint32_t tlv_server_vzw_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t    index = 0;
    uint32_t    err_code = 0;
    lwm2m_tlv_t tlv;

    while (index < p_tlv->length)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_tlv->value, p_tlv->length);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case 0: // IsRegistered
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &vzw_server_settings[instance_id].is_registered);
                break;
            }
            case 1: // ClientHoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &vzw_server_settings[instance_id].client_hold_off_timer);
                break;
            }
            default:
                break;
        }
    }

    return err_code;
}

uint32_t tlv_server_carrier_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            return tlv_server_vzw_decode(instance_id, p_tlv);

        default:
            return 0;
    }
}

/**@brief Callback function for LWM2M server instances. */
uint32_t server_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t *   p_request)
{
    LWM2M_TRC("server_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_control_access_remote_get(&access,
                                                               p_instance->object_id,
                                                               p_instance->instance_id,
                                                               p_request->remote);

    if (err_code != 0)
    {
        return err_code;
    }

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    uint16_t instance_id = p_instance->instance_id;
    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);
    uint16_t path[] = { p_instance->object_id, p_instance->instance_id, resource_id };
    uint8_t  path_len = (resource_id == LWM2M_INVALID_RESOURCE) ? ARRAY_SIZE(path) - 1 : ARRAY_SIZE(path);

    if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        uint32_t observe_option = 0;
        for (uint8_t index = 0; index < p_request->options_count; index++)
        {
            if (p_request->options[index].number == COAP_OPT_OBSERVE)
            {
                err_code = coap_opt_uint_decode(&observe_option,
                                                p_request->options[index].length,
                                                p_request->options[index].data);
                break;
            }
        }

        if (err_code == 0)
        {
            if (observe_option == 0) // Observe start
            {
                // Whitelist the resources that support observe.
                switch (resource_id)
                {
                    case LWM2M_SERVER_SHORT_SERVER_ID:
                    //case LWM2M_SERVER_LIFETIME:
                    //case LWM2M_SERVER_DEFAULT_MIN_PERIOD:
                    //case LWM2M_SERVER_DEFAULT_MAX_PERIOD:
                    //case LWM2M_SERVER_DISABLE_TIMEOUT:
                    //case LWM2M_SERVER_NOTIFY_WHEN_DISABLED:
                    //case LWM2M_SERVER_BINDING:
                    {
                        coap_message_t *p_message;

                        LWM2M_INF("Observe requested on resource %s", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
                        err_code = lwm2m_tlv_server_encode(buffer,
                                                        &buffer_size,
                                                        resource_id,
                                                        lwm2m_server_get_instance(instance_id));
                        if (err_code != 0)
                        {
                            LWM2M_INF("Failed to perform the TLV encoding");
                            lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                            return err_code;
                        }

                        err_code = lwm2m_observe_register(path, path_len, p_request, &p_message);
                        if (err_code != 0)
                        {
                            LWM2M_INF("Failed to register the observer");
                            lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                            return err_code;
                        }

                        err_code = lwm2m_coap_message_send_to_remote(p_message, p_request->remote, buffer, buffer_size);
                        if (err_code != 0)
                        {
                            LWM2M_INF("Failed to respond to Observe request");
                            lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                            return err_code;
                        }

                        lwm2m_observer_observable_init(p_request->remote, path, path_len);

                        break;
                    }

                    case LWM2M_INVALID_RESOURCE: // By design LWM2M_INVALID_RESOURCE indicates that this is on instance level.
                    default:
                    {
                        // Process the GET request as usual.
                        LWM2M_INF("Observe requested on element %s, no slots", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
                        op_code = LWM2M_OPERATION_CODE_READ;
                        break;
                    }
                }
            }
            else if (observe_option == 1) // Observe stop
            {
                if (resource_id == LWM2M_INVALID_RESOURCE) {
                    LWM2M_INF("Observe cancel on instance %s, no  match", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
                } else {
                    LWM2M_INF("Observe cancel on resource %s", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
                    const void * p_observable = lwm2m_observer_observable_get(path, path_len);
                    lwm2m_observe_unregister(p_request->remote, p_observable);
                }

                // Process the GET request as usual.
                op_code = LWM2M_OPERATION_CODE_READ;
            }
            else
            {
                (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                return 0;
            }
        }
    }

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        if (resource_id == VERIZON_RESOURCE && operator_is_vzw(true))
        {
            err_code = tlv_server_vzw_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
            err_code = lwm2m_tlv_server_encode(buffer,
                                               &buffer_size,
                                               resource_id,
                                               lwm2m_server_get_instance(instance_id));

            if (err_code == ENOENT)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                return 0;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                if (operator_is_vzw(true)) {
                    uint32_t added_size = sizeof(buffer) - buffer_size;
                    err_code = tlv_server_vzw_encode(instance_id, buffer + buffer_size, &added_size);
                    buffer_size += added_size;
                }
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        (void)lwm2m_respond_with_payload(buffer, buffer_size, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;

        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        lwm2m_time_t previous_lifetime = lwm2m_server_lifetime_get(instance_id);

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_server_decode(lwm2m_server_get_instance(instance_id),
                                               p_request->payload,
                                               p_request->payload_len,
                                               tlv_server_carrier_decode);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_server_decode(lwm2m_server_get_instance(instance_id),
                                                      resource_id,
                                                      p_request->payload,
                                                      p_request->payload_len);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_request);
            return 0;
        }

        if (lwm2m_server_lifetime_get(instance_id) != previous_lifetime)
        {
            lwm2m_request_server_instance_update(instance_id, false);
        }

        if (err_code == 0)
        {
            if (lwm2m_storage_server_store() == 0)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
            }
            else
            {
                (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            }
        }
        else if (err_code == ENOTSUP)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE_ATTR)
    {
        err_code = lwm2m_observer_write_attribute_handler(path, path_len, p_request);

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_SERVER_DISABLE:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                app_server_disable(instance_id);
                break;
            }

            case LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
#if APP_MOTIVE_FIX_UPDATE_TRIGGER
                // Use instance_id 1 when MotiveBridge say /1/0/8
                if (instance_id == 0) {
                    instance_id = 1;
                }
#endif
                lwm2m_request_server_instance_update(instance_id, false);
                break;
            }

            case LWM2M_SERVER_BOOTSTRAP_REQUEST_TRIGGER:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                lwm2m_request_bootstrap();
                break;
            }

            default:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                return 0;
            }
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        err_code = lwm2m_respond_with_instance_link(p_instance, resource_id, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        // Already handled
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

/**@brief Callback function for LWM2M server objects. */
uint32_t lwm2m_server_object_callback(lwm2m_object_t * p_object,
                                      uint16_t         instance_id,
                                      uint8_t          op_code,
                                      coap_message_t * p_request)
{
    LWM2M_TRC("server_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t           buffer_max_size = 1024;
        uint32_t           buffer_len      = buffer_max_size;
        uint8_t            buffer[buffer_max_size];
        uint32_t           index           = 0;

        uint32_t instance_buffer_len = 256;
        uint8_t  instance_buffer[instance_buffer_len];

        for (int i = 0; i < LWM2M_MAX_SERVERS+1; i++)
        {
            if (lwm2m_server_short_server_id_get(i) == 0)
            {
                continue;
            }

            uint16_t access = 0;
            lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(i);
            uint32_t err_code = lwm2m_access_control_access_remote_get(&access,
                                                                       p_instance->object_id,
                                                                       p_instance->instance_id,
                                                                       p_request->remote);
            if (err_code != 0 || (access & op_code) == 0)
            {
                continue;
            }

            instance_buffer_len = 256;
            err_code = lwm2m_tlv_server_encode(instance_buffer,
                                               &instance_buffer_len,
                                               LWM2M_NAMED_OBJECT,
                                               lwm2m_server_get_instance(i));
            if (err_code != 0)
            {
                // ENOMEM should not happen. Then it is a bug.
                break;
            }

            lwm2m_tlv_t tlv = {
                .id_type = TLV_TYPE_OBJECT,
                .id = i,
                .length = instance_buffer_len
            };
            err_code = lwm2m_tlv_header_encode(buffer + index, &buffer_len, &tlv);

            index += buffer_len;
            buffer_len = buffer_max_size - index;

            memcpy(buffer + index, instance_buffer, instance_buffer_len);

            index += instance_buffer_len;
            buffer_len = buffer_max_size - index;
        }

        err_code = lwm2m_respond_with_payload(buffer, index, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);
        // A Write-Attribute request do not contain a Content-Format option.
        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            (void)lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                          p_request->payload,
                                          p_request->payload_len,
                                          tlv_server_carrier_decode);

            m_instance_server[instance_id].proto.instance_id = instance_id;
            m_instance_server[instance_id].proto.object_id   = p_object->object_id;
            m_instance_server[instance_id].proto.callback    = server_instance_callback;

            // Cast the instance to its prototype and add it.
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_server[instance_id]);
            (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[instance_id]);
            lwm2m_access_control_instance_bind(LWM2M_OBJ_SERVER, instance_id, NULL);

            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE_ATTR)
    {
        uint16_t path[] = { p_object->object_id };
        uint8_t path_len = ARRAY_SIZE(path);

        err_code = lwm2m_observer_write_attribute_handler(path, path_len, p_request);

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_DELETE)
    {
        if (instance_id == LWM2M_INVALID_INSTANCE)
        {
            uint16_t bootstrap_ssid = lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID);
            // Delete all instances except Bootstrap server
            for (uint32_t i = 0; i < 1 + LWM2M_MAX_SERVERS; i++)
            {
                if (m_instance_server[i].short_server_id == bootstrap_ssid)
                {
                    continue;
                }

                (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_server[i]);
                lwm2m_access_control_instance_unbind(m_instance_server[i].proto.object_id, m_instance_server[i].proto.instance_id);
            }
        }
        else
        {
            if (instance_id == 0)
            {
                // Do not delete Bootstrap server
                (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                return 0;
            }

            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_server[instance_id]);
            lwm2m_access_control_instance_unbind(m_instance_server[instance_id].proto.object_id, m_instance_server[instance_id].proto.instance_id);
        }

        (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        uint16_t short_server_id = 0;
        lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);

        if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
        {
            err_code = lwm2m_respond_with_bs_discover_link(p_object->object_id, p_request);
        }
        else
        {
            err_code = lwm2m_respond_with_object_link(p_object->object_id, p_request);
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

void lwm2m_server_init(void)
{
    memset(vzw_server_settings, 0, ARRAY_SIZE(vzw_server_settings));

    m_object_server.object_id = LWM2M_OBJ_SERVER;
    m_object_server.callback = lwm2m_server_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < 1 + LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_server_init(&m_instance_server[i]);
        m_instance_server[i].proto.instance_id = i;
        m_instance_server[i].proto.callback = server_instance_callback;
    }
}

void lwm2m_server_reset(uint16_t instance_id)
{
    lwm2m_server_t * p_instance = lwm2m_server_get_instance(instance_id);

    p_instance->short_server_id = 0;
    p_instance->lifetime = 0;
    p_instance->default_minimum_period = 0;
    p_instance->default_maximum_period = 0;
    p_instance->disable_timeout = 0;
    p_instance->notification_storing_on_disabled = false;

    lwm2m_string_free(&p_instance->binding);
}

const void * lwm2m_server_resource_reference_get(uint16_t instance_id, uint16_t resource_id, uint8_t *p_type)
{
    const void *p_observable = NULL;
    uint8_t type;

    switch (resource_id)
    {
    case LWM2M_SERVER_SHORT_SERVER_ID:
        type = LWM2M_OBSERVABLE_TYPE_INT;
        p_observable = &m_instance_server[instance_id].short_server_id;
        break;
    case LWM2M_SERVER_LIFETIME:
        type = LWM2M_OBSERVABLE_TYPE_INT;
        p_observable = &m_instance_server[instance_id].lifetime;
        break;
    case LWM2M_SERVER_DEFAULT_MIN_PERIOD:
        type = LWM2M_OBSERVABLE_TYPE_INT;
        p_observable = &m_instance_server[instance_id].default_minimum_period;
        break;
    case LWM2M_SERVER_DEFAULT_MAX_PERIOD:
        type = LWM2M_OBSERVABLE_TYPE_INT;
        p_observable = &m_instance_server[instance_id].default_maximum_period;
        break;
    case LWM2M_SERVER_DISABLE_TIMEOUT:
        type = LWM2M_OBSERVABLE_TYPE_INT;
        p_observable = &m_instance_server[instance_id].disable_timeout;
        break;
    default:
        type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
    }

    if (p_type)
    {
        *p_type = type;
    }

    return p_observable;
}

uint32_t lwm2m_server_first_non_bootstrap_ssid_get(uint16_t *p_ssid)
{
    if (!p_ssid)
    {
        return EINVAL;
    }

    uint16_t bootstrap_ssid = lwm2m_security_short_server_id_get(LWM2M_BOOTSTRAP_INSTANCE_ID);

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        // Find short server id for the first instance which is not bootstrap.
        uint16_t ssid = lwm2m_server_short_server_id_get(i);
        if (ssid != 0 && ssid != bootstrap_ssid) {
            *p_ssid = ssid;
            return 0;
        }
    }

    return ENOENT;
}
