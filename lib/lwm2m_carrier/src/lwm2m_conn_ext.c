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
#include <lwm2m_conn_ext.h>
#include <coap_message.h>
#include <lwm2m_common.h>
#include <lwm2m_carrier_main.h>
#include <at_interface.h>

#define DEFAULT_APN_RETRIES               2
#define DEFAULT_APN_RETRY_PERIOD          0
#define DEFAULT_APN_RETRY_BACK_OFF_PERIOD 86400

static lwm2m_object_t                 m_object_conn_ext;        /**< AT&T Connectivity Extension base object. */
static lwm2m_connectivity_extension_t m_instance_conn_ext;      /**< AT&T Connectivity Extension object instance. */

lwm2m_connectivity_extension_t * lwm2m_conn_ext_get_instance(uint16_t instance_id)
{
    return &m_instance_conn_ext;
}

lwm2m_object_t * lwm2m_conn_ext_get_object(void)
{
    return &m_object_conn_ext;
}

/**@brief Callback function for connectivity extension instances. */
uint32_t conn_ext_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    LWM2M_TRC("conn_ext_instance_callback");

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

        err_code = lwm2m_tlv_connectivity_extension_encode(buffer,
                                                           &buffer_size,
                                                           resource_id,
                                                           &m_instance_conn_ext);
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
            err_code = lwm2m_tlv_connectivity_extension_decode(&m_instance_conn_ext,
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

/**@brief Callback function for AT&T connectivity extension objects. */
uint32_t lwm2m_conn_ext_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request)
{
    LWM2M_TRC("conn_ext_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t buffer_len = 255;
        uint8_t  buffer[buffer_len];

        err_code = lwm2m_tlv_connectivity_extension_encode(buffer + 3, &buffer_len,
                                                           LWM2M_NAMED_OBJECT,
                                                           &m_instance_conn_ext);
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

void lwm2m_conn_ext_init_acl(void)
{
    lwm2m_set_carrier_acl((lwm2m_instance_t *)&m_instance_conn_ext);
}

void lwm2m_conn_ext_init(void)
{
    //
    // AT&T Connectivity Extension instance.
    //
    lwm2m_instance_connectivity_extension_init(&m_instance_conn_ext);

    m_object_conn_ext.object_id = LWM2M_OBJ_CONN_EXT;

    m_instance_conn_ext.iccid.p_val = NULL;
    m_instance_conn_ext.iccid.len = 0;
    m_instance_conn_ext.imsi.p_val = NULL;
    m_instance_conn_ext.imsi.len = 0;
    m_instance_conn_ext.msisdn.p_val = NULL;
    m_instance_conn_ext.msisdn.len = 0;
    m_instance_conn_ext.apn_retries.val.p_uint8[0] = DEFAULT_APN_RETRIES;
    m_instance_conn_ext.apn_retries.len = 1;
    m_instance_conn_ext.apn_retry_period.val.p_int32[0] = DEFAULT_APN_RETRY_PERIOD;
    m_instance_conn_ext.apn_retry_period.len = 1;
    m_instance_conn_ext.apn_retry_back_off_period.val.p_int32[0] = DEFAULT_APN_RETRY_BACK_OFF_PERIOD;
    m_instance_conn_ext.apn_retry_back_off_period.len = 1;

    m_object_conn_ext.callback = lwm2m_conn_ext_object_callback;
    m_instance_conn_ext.proto.callback = conn_ext_instance_callback;

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_conn_ext,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    lwm2m_conn_ext_init_acl();

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_ext);
}
