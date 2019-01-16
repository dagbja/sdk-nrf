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

#include <net/coap_option.h>
#include <net/coap_observe_api.h>
#include <net/coap_message.h>

#include <common.h>

#define SECURITY_SERVER_URI_SIZE_MAX    64                                                    /**< Max size of server URIs. */
#define SECURITY_SMS_NUMBER_SIZE_MAX    20                                                    /**< Max size of server SMS number. */
#define SERVER_BINDING_SIZE_MAX         4                                                     /**< Max size of server binding. */
#define VERIZON_RESOURCE 30000

extern uint32_t app_store_bootstrap_security_values(uint16_t instance_id);

typedef struct
{
    // Security object values
    char     server_uri[SECURITY_SERVER_URI_SIZE_MAX];                 /**< Server URI to the server. */
    bool     is_bootstrap_server;
    uint8_t  sms_security_mode;
    //char     sms_binding_key_param[6];
    //char     sms_binding_secret_keys[48];
    char     sms_number[SECURITY_SMS_NUMBER_SIZE_MAX];
    time_t   client_hold_off_time;
    uint32_t hold_off_timer;
    uint32_t bootstrapped;
} security_settings_t;

static security_settings_t m_security_settings[1+LWM2M_MAX_SERVERS];

static lwm2m_object_t    m_object_security;                                 /**< LWM2M security base object. */
static lwm2m_security_t  m_instance_security[1+LWM2M_MAX_SERVERS];          /**< Security object instances. Index 0 is always bootstrap instance. */

static char *            m_public_key[1+LWM2M_MAX_SERVERS];
static char *            m_secret_key[1+LWM2M_MAX_SERVERS];


char * lwm2m_security_server_uri_get(uint16_t instance_id)
{
    return m_security_settings[instance_id].server_uri;
}

void lwm2m_security_server_uri_set(uint16_t instance_id, char * value)
{
    strncpy(m_security_settings[instance_id].server_uri, value, strlen(value));
}

uint32_t lwm2m_security_is_bootstrap_server_get(uint16_t instance_id)
{
    return m_security_settings[instance_id].is_bootstrap_server;
}

void lwm2m_security_is_bootstrap_server_set(uint16_t instance_id, bool value)
{
    m_security_settings[instance_id].is_bootstrap_server = value;
}

uint32_t lwm2m_security_bootstrapped_get(uint16_t instance_id)
{
    return m_security_settings[instance_id].bootstrapped;
}

void lwm2m_security_bootstrapped_set(uint16_t instance_id, uint32_t value)
{
    m_security_settings[instance_id].bootstrapped = value;
}

uint32_t lwm2m_security_hold_off_timer_get(uint16_t instance_id)
{
    return m_security_settings[instance_id].hold_off_timer;
}

void lwm2m_security_hold_off_timer_set(uint16_t instance_id, uint32_t value)
{
    m_security_settings[instance_id].hold_off_timer = value;
}

char * lwm2m_security_identity_get(uint16_t instance_id)
{
    return m_public_key[instance_id];
}

void lwm2m_security_identity_set(uint16_t instance_id, char * value)
{
    char * p_id = m_public_key[instance_id];
    if (p_id != NULL) {
        k_free(m_public_key[instance_id]);
    }

    if (value != NULL) {
        size_t public_key_len = m_instance_security[instance_id].public_key.len;
        m_public_key[instance_id] = k_malloc(strlen(value) + 1);
        memcpy(m_public_key[instance_id], m_instance_security[instance_id].public_key.p_val, public_key_len);
        m_public_key[instance_id][public_key_len] = 0;
    }
}

char * lwm2m_security_psk_get(uint16_t instance_id)
{
    return m_secret_key[instance_id];
}

void lwm2m_security_psk_set(uint16_t instance_id, char * value)
{
    char * p_id = m_secret_key[instance_id];
    if (p_id != NULL) {
        k_free(m_secret_key[instance_id]);
    }

    if (value != NULL) {
        size_t secret_key_len = m_instance_security[instance_id].secret_key.len * 2;
        m_secret_key[instance_id] = k_malloc(secret_key_len + 1);
        for (int i = 0; i < m_instance_security[instance_id].secret_key.len; i++) {
            sprintf(&m_secret_key[instance_id][i*2], "%02x", m_instance_security[instance_id].secret_key.p_val[i]);
        }
        m_secret_key[instance_id][secret_key_len] = 0;
    }
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
                                                         &m_security_settings[instance_id].hold_off_timer);
                break;
            }
            case 1: // IsBootstrapped
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_security_settings[instance_id].bootstrapped);
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
            printk("Unhandled security resource: %i", p_tlv->id);
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
        uint16_t instance_id = p_instance->instance_id;

        err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                             p_request->payload,
                                             p_request->payload_len,
                                             tlv_security_resource_decode);
        if (err_code != 0)
        {
            return err_code;
        }

        if (app_store_bootstrap_security_values(instance_id) == 0)
        {
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
                                                      tlv_security_resource_decode);
        if (err_code != 0)
        {
            return 0;
        }

        // Copy public key as string
        size_t public_key_len = m_instance_security[instance_id].public_key.len;
        m_public_key[instance_id] = k_malloc(public_key_len + 1);
        memcpy(m_public_key[instance_id], m_instance_security[instance_id].public_key.p_val, public_key_len);
        m_public_key[instance_id][public_key_len] = 0;

        // Convert secret key from binary to string
        size_t secret_key_len = m_instance_security[instance_id].secret_key.len * 2;
        m_secret_key[instance_id] = k_malloc(secret_key_len + 1);
        for (int i = 0; i < m_instance_security[instance_id].secret_key.len; i++) {
            sprintf(&m_secret_key[instance_id][i*2], "%02x", m_instance_security[instance_id].secret_key.p_val[i]);
        }
        m_secret_key[instance_id][secret_key_len] = 0;

        LWM2M_TRC("Secret key %d: %s", instance_id, m_secret_key[instance_id]);

        LWM2M_TRC("decoded security.");

        m_instance_security[instance_id].proto.instance_id = instance_id;
        m_instance_security[instance_id].proto.object_id   = p_object->object_id;
        m_instance_security[instance_id].proto.callback    = security_instance_callback;

        if (app_store_bootstrap_security_values(instance_id) == 0)
        {
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
            // URI was too long to be copied.
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
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
    memset(m_security_settings, 0, ARRAY_SIZE(m_security_settings));

    m_object_security.object_id = LWM2M_OBJ_SECURITY;
    m_object_security.callback = security_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_security_init(&m_instance_security[i]);
        m_instance_security[i].proto.instance_id = i;
    }
}