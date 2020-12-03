/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdio.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_access_control.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_remote.h>
#include <lwm2m_security.h>
#include <lwm2m_instance_storage.h>
#include <operator_check.h>

#include <coap_option.h>
#include <coap_observe_api.h>
#include <coap_message.h>


#define VERIZON_RESOURCE                30000

static lwm2m_object_t    m_object_security;                                 /**< LWM2M security base object. */
static lwm2m_security_t  m_instance_security[1+LWM2M_MAX_SERVERS];          /**< Security object instances. Index 0 is always bootstrap instance. */

// Verizon specific resources.
static vzw_bootstrap_security_settings_t vzw_bootstrap_security_settings;

bool lwm2m_security_bootstrapped_get(void)
{
    return vzw_bootstrap_security_settings.is_bootstrapped;
}

void lwm2m_security_bootstrapped_set(bool value)
{
    vzw_bootstrap_security_settings.is_bootstrapped = value;
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

void lwm2m_security_server_uri_set(uint16_t instance_id, const char * p_value, uint8_t len)
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

static uint32_t tlv_security_vzw_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    // The order of the list elements is important here because
    //  0 is HoldOffTimer
    //  1 is IsBootstrapped
    int32_t list_values[2] =
    {
        m_instance_security[instance_id].client_hold_off_time,
        vzw_bootstrap_security_settings.is_bootstrapped
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

uint32_t tlv_security_carrier_encode(uint16_t instance_id, uint8_t * p_buffer,
                                     uint32_t * p_buffer_len)
{
    if (!operator_is_vzw(true) || (instance_id != 0)) {
        /* Nothing to encode */
        *p_buffer_len = 0;
        return 0;
    }

    return tlv_security_vzw_encode(instance_id, p_buffer, p_buffer_len);
}

static uint32_t tlv_security_vzw_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
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
                                                         &m_instance_security[instance_id].client_hold_off_time);
                break;
            }

            case 1: // IsBootstrapped
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &vzw_bootstrap_security_settings.is_bootstrapped);
                break;
            }

            default:
                break;
        }
    }

    return err_code;
}


uint32_t tlv_security_carrier_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            return tlv_security_vzw_decode(instance_id, p_tlv);

        default:
            return 0;
    }
}

/**@brief Callback function for LWM2M security instances. */
uint32_t security_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    LWM2M_TRC("security_instance_callback");

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

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }
        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                                 p_request->payload,
                                                 p_request->payload_len,
                                                 tlv_security_carrier_decode);
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
            if (lwm2m_storage_security_store() == 0)
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
        uint32_t err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                                      p_request->payload,
                                                      p_request->payload_len,
                                                      tlv_security_carrier_decode);
        if (err_code != 0)
        {
            return 0;
        }

        m_instance_security[instance_id].proto.instance_id = instance_id;
        m_instance_security[instance_id].proto.object_id   = p_object->object_id;
        m_instance_security[instance_id].proto.callback    = security_instance_callback;

        // Cast the instance to its prototype and add it to the CoAP handler to become a
        // public instance. We can only have one so we delete the first if any.
        (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[instance_id]);
        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_security[instance_id]);

        (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_DELETE)
    {
        if (instance_id == LWM2M_INVALID_INSTANCE)
        {
            // Delete all instances except Bootstrap server
            for (uint32_t i = 1; i < 1 + LWM2M_MAX_SERVERS; i++)
            {
                (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[i]);
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

            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[instance_id]);
        }
        (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        uint16_t short_server_id = 0;
        lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);

        if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
        {
            (void)lwm2m_respond_with_bs_discover_link(p_object->object_id, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        }
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
        m_instance_security[i].proto.callback = security_instance_callback;
    }
}

void lwm2m_security_reset(uint16_t instance_id)
{
    lwm2m_security_t * p_instance = lwm2m_security_get_instance(instance_id);

    p_instance->bootstrap_server = false;
    p_instance->security_mode = 0;
    p_instance->sms_security_mode = 0;
    p_instance->short_server_id = 0;
    p_instance->client_hold_off_time = 0;

    lwm2m_string_free(&p_instance->server_uri);
    lwm2m_opaque_free(&p_instance->public_key);
    lwm2m_opaque_free(&p_instance->server_public_key);
    lwm2m_opaque_free(&p_instance->secret_key);
    lwm2m_opaque_free(&p_instance->sms_binding_key_param);
    lwm2m_opaque_free(&p_instance->sms_binding_secret_keys);
    lwm2m_string_free(&p_instance->sms_number);
}
