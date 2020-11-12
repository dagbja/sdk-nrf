/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_observer.h>
#include <lwm2m_conn_ext.h>
#include <coap_message.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_instance_storage.h>
#include <at_interface.h>
#include <lwm2m_access_control.h>
#include <lwm2m_carrier_client.h>

#define DEFAULT_APN_RETRIES               2
#define DEFAULT_APN_RETRY_PERIOD          0
#define DEFAULT_APN_RETRY_BACK_OFF_PERIOD 86400

static lwm2m_object_t                 m_object_conn_ext;        /**< AT&T Connectivity Extension base object. */
static lwm2m_connectivity_extension_t m_instance_conn_ext;      /**< AT&T Connectivity Extension object instance. */

// Forward declare
static void lwm2m_conn_ext_update_resource(uint16_t resource_id);

char * lwm2m_conn_ext_msisdn_get(uint8_t * p_len)
{
    *p_len = m_instance_conn_ext.msisdn.len;
    return m_instance_conn_ext.msisdn.p_val;
}

void lwm2m_conn_ext_msisdn_set(char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_string(p_value, len, &m_instance_conn_ext.msisdn) != 0)
    {
        LWM2M_ERR("Could not set MSISDN");
    }

    lwm2m_storage_conn_ext_store();
}

uint8_t lwm2m_conn_ext_apn_retries_get(uint16_t instance_id, uint16_t apn_instance)
{
    if (apn_instance >= m_instance_conn_ext.apn_retries.max_len) {
        return 0;
    }

    return m_instance_conn_ext.apn_retries.val.p_uint8[apn_instance];
}

int32_t lwm2m_conn_ext_apn_retry_period_get(uint16_t instance_id, uint16_t apn_instance)
{
    if (apn_instance >= m_instance_conn_ext.apn_retry_period.max_len) {
        return 0;
    }

    return m_instance_conn_ext.apn_retry_period.val.p_int32[apn_instance];
}

int32_t lwm2m_conn_ext_apn_retry_back_off_period_get(uint16_t instance_id, uint16_t apn_instance)
{
    if (apn_instance >= m_instance_conn_ext.apn_retry_back_off_period.max_len) {
        return 0;
    }

    return m_instance_conn_ext.apn_retry_back_off_period.val.p_int32[apn_instance];
}

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

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        lwm2m_conn_ext_update_resource(resource_id);

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

        char previous_msisdn[16];
        memcpy(previous_msisdn, m_instance_conn_ext.msisdn.p_val, m_instance_conn_ext.msisdn.len);

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

        if (memcmp(previous_msisdn, m_instance_conn_ext.msisdn.p_val, m_instance_conn_ext.msisdn.len) != 0)
        {
            lwm2m_last_used_msisdn_set(m_instance_conn_ext.msisdn.p_val, m_instance_conn_ext.msisdn.len);

            for (uint32_t i = 0; i < 1 + LWM2M_MAX_SERVERS; i++)
            {
                lwm2m_client_update(i);
            }
        }

        if (err_code == 0)
        {
            if (lwm2m_storage_conn_ext_store() == 0)
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

        lwm2m_conn_ext_update_resource(LWM2M_NAMED_OBJECT);
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

static int lwm2m_conn_ext_iccid_update(void)
{
    int ret;
    char iccid[20];
    uint32_t iccid_len = sizeof(iccid);

    ret = at_read_sim_iccid(iccid, &iccid_len);

    if (ret != 0)
    {
        LWM2M_WRN("Failed to read the SIM ICCID");
        return ret;
    }

    return lwm2m_bytebuffer_to_string(iccid, iccid_len, &m_instance_conn_ext.iccid);
}

static int lwm2m_conn_ext_imsi_update(void)
{
    int ret;

    ret = at_read_imsi(&m_instance_conn_ext.imsi);

    if (ret != 0)
    {
        LWM2M_WRN("Failed to read the IMSI");
    }

    return ret;
}

static int lwm2m_conn_ext_sinr_and_srxlev_update(void)
{
    int ret;

    ret = at_read_sinr_and_srxlev(&m_instance_conn_ext.sinr,
                                  &m_instance_conn_ext.srxlev);

    if (ret != 0)
    {
        LWM2M_WRN("Failed to read the SINR and/or the SRXLEV");
    }

    return ret;
}

/* Fetch latest resource value */
static void lwm2m_conn_ext_update_resource(uint16_t resource_id)
{
    switch (resource_id) {
    case LWM2M_CONN_EXT_ICCID:
        lwm2m_conn_ext_iccid_update();
        break;
    case LWM2M_CONN_EXT_IMSI:
        lwm2m_conn_ext_imsi_update();
        break;
    case LWM2M_CONN_EXT_SINR:
    case LWM2M_CONN_EXT_SRXLEV:
        lwm2m_conn_ext_sinr_and_srxlev_update();
        break;
    case LWM2M_NAMED_OBJECT:
        lwm2m_conn_ext_iccid_update();
        lwm2m_conn_ext_imsi_update();
        lwm2m_conn_ext_sinr_and_srxlev_update();
        break;
    default:
        break;
    }
}

void lwm2m_conn_ext_init(void)
{
    int32_t len;
    char last_used_msisdn[16];

    lwm2m_instance_connectivity_extension_init(&m_instance_conn_ext);

    m_object_conn_ext.object_id = LWM2M_OBJ_CONN_EXT;
    m_object_conn_ext.callback = lwm2m_conn_ext_object_callback;
    m_instance_conn_ext.proto.callback = conn_ext_instance_callback;

    len = lwm2m_last_used_msisdn_get(last_used_msisdn, sizeof(last_used_msisdn));
    if (len > 0) {
        lwm2m_bytebuffer_to_string(last_used_msisdn, len, &m_instance_conn_ext.msisdn);
    }

    lwm2m_conn_ext_iccid_update();
    lwm2m_conn_ext_imsi_update();
    m_instance_conn_ext.apn_retries.val.p_uint8[0] = DEFAULT_APN_RETRIES;
    m_instance_conn_ext.apn_retries.len = 1;
    m_instance_conn_ext.apn_retry_period.val.p_int32[0] = DEFAULT_APN_RETRY_PERIOD;
    m_instance_conn_ext.apn_retry_period.len = 1;
    m_instance_conn_ext.apn_retry_back_off_period.val.p_int32[0] = DEFAULT_APN_RETRY_BACK_OFF_PERIOD;
    m_instance_conn_ext.apn_retry_back_off_period.len = 1;
    lwm2m_conn_ext_sinr_and_srxlev_update();
    // The only CE levels supported currently are 0 and 1 (Mode A)
    lwm2m_bytebuffer_to_string("Mode A", strlen("Mode A"), &m_instance_conn_ext.ce_mode);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_ext);
}
