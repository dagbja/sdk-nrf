/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_server

#include <stdint.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>

#include <net/coap_option.h>
#include <net/coap_observe_api.h>
#include <net/coap_message.h>

#include <common.h>

extern void app_read_flash_storage(void);
extern void app_server_update(uint16_t instance_id);
extern uint32_t app_store_bootstrap_server_values(uint16_t instance_id);

#define SERVER_BINDING_SIZE_MAX         4                                                     /**< Max size of server binding. */
#define VERIZON_RESOURCE 30000

#define APP_MOTIVE_FIX_UPDATE_TRIGGER   1 // To adjust for MotiveBridge posting /1/0/8 instead of /1/1/8

static lwm2m_server_t                      m_instance_server[1+LWM2M_MAX_SERVERS];            /**< Server object instance to be filled by the bootstrap server. */
static lwm2m_object_t                      m_object_server;                                   /**< LWM2M server base object. */

// Local values
typedef struct {
    union {
        uint32_t bootstrapped;
        uint32_t registered;
    } is;
    uint16_t short_server_id;
    uint32_t hold_off_timer;      /**< The number of seconds to wait before attempting bootstrap or registration. */
    time_t   lifetime;
    time_t   default_minimum_period;
    time_t   default_maximum_period;
    time_t   disable_timeout;
    bool     notification_storing_on_disabled;
    char     binding[SERVER_BINDING_SIZE_MAX];
} server_settings_t;

static server_settings_t m_server_settings[1+LWM2M_MAX_SERVERS];

uint32_t lwm2m_server_registered_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].is.registered;
}

void lwm2m_server_registered_set(uint16_t instance_id, uint32_t value)
{
    m_server_settings[instance_id].is.registered = value;
}

time_t lwm2m_server_lifetime_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].lifetime;
}

void lwm2m_server_lifetime_set(uint16_t instance_id, time_t value)
{
    time_t previous = m_server_settings[instance_id].lifetime = value;
    m_server_settings[instance_id].lifetime = value;
    if ((value != previous) && m_server_settings[instance_id].is.registered)
    {
        app_server_update(instance_id);
    }
}

time_t lwm2m_server_min_period_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].default_minimum_period;
}

void lwm2m_server_min_period_set(uint16_t instance_id, time_t value)
{
    m_server_settings[instance_id].default_minimum_period = value;
}

time_t lwm2m_server_max_period_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].default_maximum_period;
}

void lwm2m_server_max_period_set(uint16_t instance_id, time_t value)
{
    m_server_settings[instance_id].default_maximum_period = value;
}

time_t lwm2m_server_disable_timeout_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].disable_timeout;
}

void lwm2m_server_disable_timeout_set(uint16_t instance_id, time_t value)
{
    m_server_settings[instance_id].disable_timeout = value;
}

bool lwm2m_server_notif_storing_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].notification_storing_on_disabled;
}

void lwm2m_server_notif_storing_set(uint16_t instance_id, bool value)
{
    m_server_settings[instance_id].notification_storing_on_disabled = value;
}

char * lwm2m_server_binding_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].binding;
}

void lwm2m_server_binding_set(uint16_t instance_id, char * value)
{
    strncpy(m_server_settings[instance_id].binding, value, strlen(value));
}

uint32_t lwm2m_server_hold_off_timer_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].hold_off_timer;
}

void lwm2m_server_hold_off_timer_set(uint16_t instance_id, uint32_t value)
{
    m_server_settings[instance_id].hold_off_timer = value;
}

uint16_t lwm2m_server_short_server_id_get(uint16_t instance_id)
{
    return m_server_settings[instance_id].short_server_id;
}

void lwm2m_server_short_server_id_set(uint16_t instance_id, uint16_t value)
{
    m_server_settings[instance_id].short_server_id = value;
}

lwm2m_server_t * lwm2m_server_get_instance(uint16_t instance_id)
{
    return &m_instance_server[instance_id];
}

lwm2m_object_t * lwm2m_server_get_object(void)
{
    return &m_object_server;
}

uint32_t lwm2m_server_observer_process(void)
{
    return 0;
}

static uint32_t tlv_server_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    int32_t list_values[2] =
    {
        m_server_settings[instance_id].is.registered,
        m_server_settings[instance_id].hold_off_timer
    };

    lwm2m_list_t list =
    {
        .type        = LWM2M_LIST_TYPE_INT32,
        .p_id        = NULL,
        .val.p_int32 = list_values,
        .len         = 2
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}


static uint32_t tlv_server_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
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
                                                         &m_server_settings[instance_id].is.registered);
                break;
            }
            case 1: // ClientHoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_server_settings[instance_id].hold_off_timer);
                break;
            }
            default:
                break;
        }
    }

    return err_code;
}

uint32_t tlv_server_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_server_verizon_decode(instance_id, p_tlv);
            break;

        default:
#if 0
            err_code = ENOENT;
#else
            printk("Unhandled server resource: %i", p_tlv->id);
#endif
            break;
    }

    return err_code;
}

/**@brief Callback function for LWM2M server instances. */
uint32_t server_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t *   p_request)
{
    LWM2M_TRC("server_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = common_lwm2m_access_remote_get(&access,
                                                       p_instance,
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

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_server_verizon_encode(instance_id, buffer, &buffer_size);
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
                uint32_t added_size = sizeof(buffer) - buffer_size;
                err_code = tlv_server_verizon_encode(instance_id, buffer + buffer_size, &added_size);
                buffer_size += added_size;
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
        u32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_server_decode(lwm2m_server_get_instance(instance_id),
                                               p_request->payload,
                                               p_request->payload_len,
                                               tlv_server_resource_decode);
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

        if (err_code == 0)
        {
            if (app_store_bootstrap_server_values(instance_id) == 0)
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
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_SERVER_DISABLE:
            {
                // TODO: Disconnect, wait disable_timeout seconds and reconnect.
                (void)lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
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
                app_server_update(instance_id);
                break;
            }

            default:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                return 0;
            }
        }
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

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        (void)lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                      p_request->payload,
                                      p_request->payload_len,
                                      tlv_server_resource_decode);

        m_instance_server[instance_id].proto.instance_id = instance_id;
        m_instance_server[instance_id].proto.object_id   = p_object->object_id;
        m_instance_server[instance_id].proto.callback    = server_instance_callback;

        if (app_store_bootstrap_server_values(instance_id) == 0)
        {
            // Cast the instance to its prototype and add it.
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_server[instance_id]);
            (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[instance_id]);

            // Initialize ACL on the instance
            // The owner (second parameter) is set to LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID.
            // This will grant the Bootstrap server full permission to this instance.
            (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[instance_id],
                                            LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
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

    memset(m_server_settings, 0, ARRAY_SIZE(m_server_settings));

    m_object_server.object_id = LWM2M_OBJ_SERVER;
    m_object_server.callback = lwm2m_server_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_server_init(&m_instance_server[i]);
        m_instance_server[i].proto.instance_id = i;
    }
    
    app_read_flash_storage();

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        m_instance_server[i].short_server_id = m_server_settings[i].short_server_id;
        m_instance_server[i].lifetime = m_server_settings[i].lifetime;
        m_instance_server[i].default_minimum_period = m_server_settings[i].default_minimum_period;
        m_instance_server[i].default_maximum_period = m_server_settings[i].default_maximum_period;
        m_instance_server[i].disable_timeout = m_server_settings[i].disable_timeout;
        m_instance_server[i].notification_storing_on_disabled = m_server_settings[i].notification_storing_on_disabled;

        (void)lwm2m_bytebuffer_to_string(m_server_settings[i].binding,
                                         strlen(m_server_settings[i].binding),
                                         &m_instance_server[i].binding);

        m_instance_server[i].proto.callback = server_instance_callback;

        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[i]);
    }

}
