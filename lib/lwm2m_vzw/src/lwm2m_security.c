/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_security

#include <stdint.h>
#include <stdio.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_security.h>
#include <lwm2m_instance_storage.h>

#include <coap_option.h>
#include <coap_observe_api.h>
#include <coap_message.h>

#include <common.h>

#define VERIZON_RESOURCE                30000

static lwm2m_object_t    m_object_security;                                 /**< LWM2M security base object. */
static lwm2m_security_t  m_instance_security[1+LWM2M_MAX_SERVERS];          /**< Security object instances. Index 0 is always bootstrap instance. */

// Verizon specific resources.
static vzw_bootstrap_security_settings_t vzw_boostrap_security_settings;

bool lwm2m_security_bootstrapped_get(uint16_t instance_id)
{
    return vzw_boostrap_security_settings.is_bootstrapped;
}

void lwm2m_security_bootstrapped_set(uint16_t instance_id, bool value)
{
    vzw_boostrap_security_settings.is_bootstrapped = value;
}

int32_t lwm2m_security_hold_off_timer_get(uint16_t instance_id)
{
    return vzw_boostrap_security_settings.hold_off_timer;
}

void lwm2m_security_hold_off_timer_set(uint16_t instance_id, int32_t value)
{
    vzw_boostrap_security_settings.hold_off_timer = value;
}

// LWM2M core resources.
int32_t lwm2m_security_client_hold_off_time_get(uint16_t instance_id)
{
    return m_instance_security[instance_id].client_hold_off_time;
}

void lwm2m_security_client_hold_off_time_set(uint16_t instance_id, int32_t value)
{
    m_instance_security[instance_id].client_hold_off_time = value;
}

char * lwm2m_security_server_uri_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_security[instance_id].server_uri.len;
    return m_instance_security[instance_id].server_uri.p_val;
}

void lwm2m_security_server_uri_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_string(p_value, len, &m_instance_security[instance_id].server_uri) != 0)
    {
        LWM2M_ERR("Could not set server URI");
    }
}

bool lwm2m_security_is_bootstrap_server_get(uint16_t instance_id)
{
    return m_instance_security[instance_id].bootstrap_server;
}

void lwm2m_security_is_bootstrap_server_set(uint16_t instance_id, bool value)
{
    m_instance_security[instance_id].bootstrap_server = value;
}

char * lwm2m_security_identity_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_security[instance_id].public_key.len;
    return m_instance_security[instance_id].public_key.p_val;
}

void lwm2m_security_identity_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_opaque(p_value, len, &m_instance_security[instance_id].public_key) != 0)
    {
        LWM2M_ERR("Could not set identity");
    }
}

char * lwm2m_security_psk_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_security[instance_id].secret_key.len;
    return m_instance_security[instance_id].secret_key.p_val;
}

void lwm2m_security_psk_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_opaque(p_value, len, &m_instance_security[instance_id].secret_key) != 0)
    {
        LWM2M_ERR("Could not set PSK");
    }
}

char * lwm2m_security_sms_number_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_security[instance_id].sms_number.len;
    return m_instance_security[instance_id].sms_number.p_val;
}

void lwm2m_security_sms_number_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_string(p_value, len, &m_instance_security[instance_id].sms_number) != 0)
    {
        LWM2M_ERR("Could not set SMS number");
    }
}

uint16_t lwm2m_security_short_server_id_get(uint16_t instance_id)
{
    return m_instance_security[instance_id].short_server_id;
}

void lwm2m_security_short_server_id_set(uint16_t instance_id, uint16_t value)
{
    m_instance_security[instance_id].short_server_id = value;
}

static uint32_t tlv_security_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
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
            case 0: // HoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &vzw_boostrap_security_settings.hold_off_timer);
                break;
            }

            case 1: // IsBootstrapped
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &vzw_boostrap_security_settings.is_bootstrapped);
                break;
            }

            default:
                break;
        }
    }

    return err_code;
}


static uint32_t tlv_security_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code = 0;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_security_verizon_decode(instance_id, p_tlv);
            break;

        default:
#if 0
            err_code = ENOENT;
#else
            LWM2M_ERR("Unhandled security resource: %i", p_tlv->id);
#endif
            break;
    }

    return err_code;
}

/**@brief Callback function for LWM2M security instances. */
uint32_t security_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    LWM2M_TRC("security_instance_callback");

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

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
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
            uint16_t instance_id = p_instance->instance_id;

            lwm2m_security_t unpack_struct;
            memset(&unpack_struct, 0, sizeof(lwm2m_firmware_t));

            err_code = lwm2m_tlv_security_decode(&unpack_struct,
                                                p_request->payload,
                                                p_request->payload_len,
                                                NULL);
            if (err_code != 0)
            {
                return err_code;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                // Write the whole object instance.
                lwm2m_security_server_uri_set(instance_id, unpack_struct.server_uri.p_val, unpack_struct.server_uri.len);
                lwm2m_security_identity_set(instance_id, unpack_struct.public_key.p_val, unpack_struct.public_key.len);
                lwm2m_security_psk_set(instance_id, unpack_struct.secret_key.p_val, unpack_struct.secret_key.len);
            }
            else
            {
                switch (resource_id)
                {
                    case LWM2M_SECURITY_SERVER_URI:
                    {
                        lwm2m_security_server_uri_set(instance_id, unpack_struct.server_uri.p_val, unpack_struct.server_uri.len);
                        break;
                    }

                    case LWM2M_SECURITY_PUBLIC_KEY:
                    {
                        lwm2m_security_identity_set(instance_id, unpack_struct.public_key.p_val, unpack_struct.public_key.len);
                        break;
                    }

                    case LWM2M_SECURITY_SECRET_KEY:
                    {
                        lwm2m_security_psk_set(instance_id, unpack_struct.secret_key.p_val, unpack_struct.secret_key.len);
                        break;
                    }

                    default:
                        // Default to BAD_REQUEST error.
                        err_code = EINVAL;
                        break;
                }
            }
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_request);
            return 0;
        }

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
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
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return 0;
}


/**@brief Callback function for LWM2M object instances. */
uint32_t security_object_callback(lwm2m_object_t  * p_object,
                                  uint16_t          instance_id,
                                  uint8_t           op_code,
                                  coap_message_t *  p_request)
{
    LWM2M_TRC("security_object_callback, instance %u", instance_id);

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        lwm2m_security_t unpack_struct;
        memset(&unpack_struct, 0, sizeof(lwm2m_security_t));

        uint32_t err_code = lwm2m_tlv_security_decode(&unpack_struct,
                                                      p_request->payload,
                                                      p_request->payload_len,
                                                      tlv_security_resource_decode);
        if (err_code != 0)
        {
            return 0;
        }

        // Copy fields from parsed TLV to the instance.
        // Server URI.
        lwm2m_security_server_uri_set(instance_id, unpack_struct.server_uri.p_val, unpack_struct.server_uri.len);
        // identity.
        lwm2m_security_identity_set(instance_id, unpack_struct.public_key.p_val, unpack_struct.public_key.len);
        // PSK.
        lwm2m_security_psk_set(instance_id, unpack_struct.secret_key.p_val, unpack_struct.secret_key.len);

        m_instance_security[instance_id].proto.instance_id = instance_id;
        m_instance_security[instance_id].proto.object_id   = p_object->object_id;
        m_instance_security[instance_id].proto.callback    = security_instance_callback;

        // No ACL object for security objects.
        ((lwm2m_instance_t *)&m_instance_security[instance_id])->acl.id = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;

        // Cast the instance to its prototype and add it to the CoAP handler to become a
        // public instance. We can only have one so we delete the first if any.
        (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[instance_id]);

        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_security[instance_id]);

        (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return 0;
}

lwm2m_security_t * lwm2m_security_get_instance(uint16_t instance_id)
{
    return &m_instance_security[instance_id];
}

lwm2m_object_t * lwm2m_security_get_object(void)
{
    return &m_object_security;
}

void lwm2m_security_init(void)
{
    m_object_security.object_id = LWM2M_OBJ_SECURITY;
    m_object_security.callback = security_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_security_init(&m_instance_security[i]);
        m_instance_security[i].proto.instance_id = i;
    }
}