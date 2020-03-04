/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_apn_conn_prof.h>
#include <coap_message.h>
#include <lwm2m_common.h>
#include <lwm2m_carrier_main.h>
#include <at_interface.h>

#define DEFAULT_PROFILE_NAME     "AT&T LWM2M APN"
#define DEFAULT_APN              "attm2mglobal"

static lwm2m_object_t        m_object_apn_conn_prof;        /**< APN Connection Profile base object. */
static lwm2m_apn_conn_prof_t m_instance_apn_conn_prof;      /**< APN Connection Profile object instance. */

// LWM2M core resources.

lwm2m_apn_conn_prof_t * lwm2m_apn_conn_prof_get_instance(uint16_t instance_id)
{
    return &m_instance_apn_conn_prof;
}

lwm2m_object_t * lwm2m_apn_conn_prof_get_object(void)
{
    return &m_object_apn_conn_prof;
}

/**@brief Callback function for APN connection profile instances. */
uint32_t apn_conn_prof_instance_callback(lwm2m_instance_t * p_instance,
                                         uint16_t           resource_id,
                                         uint8_t            op_code,
                                         coap_message_t *   p_request)
{
    LWM2M_TRC("apn_conn_prof_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_remote_get(&access,
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

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {

        err_code = lwm2m_tlv_apn_connection_profile_encode(buffer,
                                                           &buffer_size,
                                                           resource_id,
                                                           &m_instance_apn_conn_prof);
        if (err_code == ENOENT)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return 0;
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

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_apn_connection_profile_decode(&m_instance_apn_conn_prof,
                                                                p_request->payload,
                                                                p_request->payload_len,
                                                                NULL);
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
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        err_code = lwm2m_respond_with_instance_link(p_instance, resource_id, p_request);
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

/**@brief Callback function for LWM2M APN connection profile objects. */
uint32_t lwm2m_apn_conn_prof_object_callback(lwm2m_object_t * p_object,
                                             uint16_t         instance_id,
                                             uint8_t          op_code,
                                             coap_message_t * p_request)
{
    LWM2M_TRC("apn_connection_profile_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t buffer_len = 255;
        uint8_t  buffer[buffer_len];

        err_code = lwm2m_tlv_apn_connection_profile_encode(buffer + 3, &buffer_len,
                                                           LWM2M_NAMED_OBJECT,
                                                           &m_instance_apn_conn_prof);
        lwm2m_tlv_t tlv = {
            .id_type = TLV_TYPE_OBJECT,
            .length = buffer_len
        };
        err_code = lwm2m_tlv_header_encode(buffer, &buffer_len, &tlv);
        buffer_len += tlv.length;

        err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        err_code = lwm2m_respond_with_object_link(p_object->object_id, p_request);
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

void lwm2m_apn_conn_prof_init_acl(void)
{
    lwm2m_set_carrier_acl((lwm2m_instance_t *)&m_instance_apn_conn_prof);
}

void lwm2m_apn_conn_prof_init(void)
{
    //
    // APN Connection Profile instance.
    //
    lwm2m_instance_apn_connection_profile_init(&m_instance_apn_conn_prof);

    m_object_apn_conn_prof.object_id = LWM2M_OBJ_APN_CONNECTION_PROFILE;

    (void)lwm2m_bytebuffer_to_string(DEFAULT_PROFILE_NAME, strlen(DEFAULT_PROFILE_NAME), &m_instance_apn_conn_prof.profile_name);
    (void)lwm2m_bytebuffer_to_string(DEFAULT_APN, strlen(DEFAULT_APN), &m_instance_apn_conn_prof.apn);
    m_instance_apn_conn_prof.enable_status = true;
    m_instance_apn_conn_prof.authentication_type = 0;

    m_object_apn_conn_prof.callback = lwm2m_apn_conn_prof_object_callback;
    m_instance_apn_conn_prof.proto.callback = apn_conn_prof_instance_callback;

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_apn_conn_prof,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    lwm2m_apn_conn_prof_init_acl();

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_apn_conn_prof);
}
