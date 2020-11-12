/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdlib.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_access_control.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_observer.h>
#include <lwm2m_conn_mon.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_apn_conn_prof.h>
#include <coap_message.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_pdn.h>
#include <operator_check.h>
#include <at_interface.h>
#include <app_debug.h>
#include <lwm2m_remote.h>
#include <lwm2m_os.h>

#define VERIZON_RESOURCE            30000

static lwm2m_object_t                  m_object_conn_mon;        /**< Connectivity Monitoring base object. */
static lwm2m_connectivity_monitoring_t m_instance_conn_mon;      /**< Connectivity Monitoring object instance. */
static vzw_conn_mon_class_apn_t        m_vzw_conn_mon_class_apn; /**< Verizon specific APN names. */

static char m_apn_class_scratch_buffer[64];
// Forward declare.
static void lwm2m_conn_mon_update_resource(uint16_t resource_id);

// Verizon specific resources.

static bool operation_is_allowed(uint16_t res, uint16_t op)
{
    if (res < ARRAY_SIZE(m_instance_conn_mon.operations)) {
        return m_instance_conn_mon.operations[res] & op;
    }

    /* Allow by default, it could be a carrier-specific resource */
    return true;
}

static int8_t class_apn_index(uint8_t apn_class)
{
    int8_t apn_index = -1;

    switch (apn_class) {
    case 2:
        apn_index = LWM2M_CONN_MON_30000_CLASS_APN_2;
        break;
    case 3:
        apn_index = LWM2M_CONN_MON_30000_CLASS_APN_3;
        break;
    case 6:
        apn_index = LWM2M_CONN_MON_30000_CLASS_APN_6;
        break;
    case 7:
        apn_index = LWM2M_CONN_MON_30000_CLASS_APN_7;
        break;
    default:
        break;
    }

    return apn_index;
}

static int8_t index_apn_class(uint8_t apn_index)
{
    int8_t apn_class = -1;

    switch (apn_index) {
    case LWM2M_CONN_MON_30000_CLASS_APN_2:
        apn_class = 2;
        break;
    case LWM2M_CONN_MON_30000_CLASS_APN_3:
        apn_class = 3;
        break;
    case LWM2M_CONN_MON_30000_CLASS_APN_6:
        apn_class = 6;
        break;
    case LWM2M_CONN_MON_30000_CLASS_APN_7:
        apn_class = 7;
        break;
    default:
        break;
    }

    return apn_class;
}

char * lwm2m_conn_mon_class_apn_get(uint8_t apn_class, uint8_t * p_len)
{
    // Check for updated value.
    size_t apn_class_len = sizeof(m_apn_class_scratch_buffer);
    int8_t apn_index = class_apn_index(apn_class);

    if (apn_index < 0 || apn_index > 3)
    {
        *p_len = 0;
        return NULL;
    }

    int result = at_read_apn_class(apn_class, m_apn_class_scratch_buffer, &apn_class_len);

    if (result == 0)
    {
        // Check if length or value has changed.
        if ((m_vzw_conn_mon_class_apn.class_apn[apn_index].len != apn_class_len) ||
            (strncmp(m_vzw_conn_mon_class_apn.class_apn[apn_index].p_val, m_apn_class_scratch_buffer, apn_class_len)))
        {
            // If changed, update the cached value, and notify new value.
            if (lwm2m_bytebuffer_to_string(m_apn_class_scratch_buffer, apn_class_len, &m_vzw_conn_mon_class_apn.class_apn[apn_index]) != 0)
            {
                LWM2M_ERR("Could not get local cached CLASS%d APN", apn_class);
            }
            else
            {
                // TODO: Value is different from previous value, shut down all sockets on the APN and attach the APN then create sockets again.
                // TODO: Value is different from previous value, do a notification.
                // lwm2m_conn_mon_notify_resource(apn_index);
            }

            // Update APN resource in case Class 2
            if (apn_class == 2)
            {
                (void) lwm2m_bytebuffer_to_string(m_apn_class_scratch_buffer, apn_class_len, &m_instance_conn_mon.apn.val.p_string[0]);
                m_instance_conn_mon.apn.len = 1;
            }
        }
    }

    *p_len = m_vzw_conn_mon_class_apn.class_apn[apn_index].len;
    return m_vzw_conn_mon_class_apn.class_apn[apn_index].p_val;
}

void lwm2m_conn_mon_class_apn_set(uint8_t apn_class, char * p_value, uint8_t len)
{
    int8_t apn_index = class_apn_index(apn_class);

    // Class2 not supported in set
    if (apn_class == 2 || apn_index < 0 || apn_index > 3)
    {
        return;
    }

    // Check if length or value has changed.
    if ((m_vzw_conn_mon_class_apn.class_apn[apn_index].len != len) ||
        (strncmp(m_vzw_conn_mon_class_apn.class_apn[apn_index].p_val, p_value, len)))
    {
        int result = at_write_apn_class(apn_class, p_value, len);

        // Update the cached value.
        if (result == 0)
        {
            if (lwm2m_bytebuffer_to_string(m_apn_class_scratch_buffer, len, &m_vzw_conn_mon_class_apn.class_apn[apn_index]) != 0)
            {
                LWM2M_ERR("Could not set local cached CLASS%d APN", apn_class);
            }
            else
            {
                // TODO: Value is different from previous value, shut down all sockets on the APN and attach the APN then create sockets again.
                // TODO: Value is different from previous value, do a notification.
                // lwm2m_conn_mon_notify_resource(apn_index);
            }

            if (apn_class == 3) {
                lwm2m_stored_class3_apn_write(p_value, len);
            }
        }
    }
}

// LWM2M core resources.

lwm2m_connectivity_monitoring_t * lwm2m_conn_mon_get_instance(uint16_t instance_id)
{
    return &m_instance_conn_mon;
}

lwm2m_object_t * lwm2m_conn_mon_get_object(void)
{
    return &m_object_conn_mon;
}

static uint32_t tlv_conn_mon_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    ARG_UNUSED(instance_id);

    // Refresh list of APN Class.
    uint8_t len = 0;
    (void)lwm2m_conn_mon_class_apn_get(2, &len);
    (void)lwm2m_conn_mon_class_apn_get(3, &len);
    (void)lwm2m_conn_mon_class_apn_get(6, &len);
    (void)lwm2m_conn_mon_class_apn_get(7, &len);

    lwm2m_list_t list =
    {
        .type         = LWM2M_LIST_TYPE_STRING,
        .val.p_string = m_vzw_conn_mon_class_apn.class_apn,
        .len          = 4,
        .max_len      = ARRAY_SIZE(m_vzw_conn_mon_class_apn.class_apn)
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}


uint32_t tlv_conn_mon_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
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
            case 0: // Class 2 APN
            {
                // READ only
                err_code = ENOENT;
                break;
            }

            case 1: // Class 3 APN for Internet
            case 2: // Class 6 APN for Enterprise
            case 3: // Class 7 APN for Thingspace
            {
                uint8_t apn_class = index_apn_class(tlv.id);
                lwm2m_conn_mon_class_apn_set(apn_class, (char *)tlv.value, tlv.length);
                break;
            }

            default:
                err_code = ENOENT;
                break;
        }
    }

    return err_code;
}


uint32_t tlv_conn_mon_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_conn_mon_verizon_decode(instance_id, p_tlv);
            break;

        default:
            err_code = ENOENT;
            break;
    }

    return err_code;
}

static void on_read(const uint16_t path[3], uint8_t path_len,
                    coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;

    const uint16_t res = path[2];

    len = sizeof(buf);

    if (res == VERIZON_RESOURCE && operator_is_vzw(true)) {
        err = tlv_conn_mon_verizon_encode(0, buf, &len);
        if (err) {
            goto reply_error;
        }

        lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
        return;
    }

    /* Update requested resource */
    lwm2m_conn_mon_update_resource(res);

    err = lwm2m_tlv_connectivity_monitoring_encode(buf, &len, res, &m_instance_conn_mon);
    if (err) {
        goto reply_error;
    }

    /* Append VzW resource */
    if (res == LWM2M_NAMED_OBJECT && operator_is_vzw(true)) {
        size_t plen = sizeof(buf) - len;
        err = tlv_conn_mon_verizon_encode(0, buf + len, &plen);
        if (err) {
           goto reply_error;
        }
        len += plen;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
    return;

reply_error: {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
    }
}

static void on_observe_start(const uint16_t path[3], uint8_t path_len,
                             coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;
    coap_message_t *p_rsp;

    len = sizeof(buf);

    LWM2M_INF("Observe register %s",
              lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));

    err = lwm2m_tlv_element_encode(buf, &len, path, path_len);
    if (err) {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_400_BAD_REQUEST;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    err = lwm2m_observe_register(path, path_len, p_req, &p_rsp);
    if (err) {
        LWM2M_WRN("Failed to register observer, err %d", err);
        lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_req);
        return;
    }

    err = lwm2m_coap_message_send_to_remote(p_rsp, p_req->remote, buf, len);
    if (err) {
        LWM2M_WRN("Failed to respond to Observe request");
        lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_req);
        return;
    }

    err = lwm2m_observer_observable_init(p_req->remote, path, path_len);
    if (err) {
        /* Already logged */
    }

    return;
}

static void on_observe_stop(const uint16_t path[3], uint8_t path_len,
                            coap_message_t *p_req)
{
    uint32_t err;

    const void *p_observable = lwm2m_observer_observable_get(path, path_len);

    LWM2M_INF("Observe deregister %s",
              lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));

    err = lwm2m_observe_unregister(p_req->remote, p_observable);
    if (err) {
        /* TODO */
    }

    /* Process as a read */
    on_read(path, path_len, p_req);
}

static void on_observe(const uint16_t path[3], uint8_t path_len,
                       coap_message_t *p_req)
{
    uint32_t err = 1;
    uint32_t opt;

    for (uint8_t i = 0; i < p_req->options_count; i++) {
        if (p_req->options[i].number == COAP_OPT_OBSERVE) {
            err = coap_opt_uint_decode(&opt,
                           p_req->options[i].length,
                           p_req->options[i].data);
            break;
        }
    }

    if (err) {
        lwm2m_respond_with_code(COAP_CODE_402_BAD_OPTION, p_req);
        return;
    }

    switch (opt) {
    case 0: /* observe start */
        on_observe_start(path, path_len, p_req);
        break;
    case 1: /* observe stop */
        on_observe_stop(path, path_len, p_req);
        break;
    default:
        /* Unexpected opt value */
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        break;
    }
}

static void on_write_attribute(const uint16_t path[3], uint8_t path_len,
                               coap_message_t *p_req)
{
    int err;

    err = lwm2m_observer_write_attribute_handler(path, path_len, p_req);
    if (err) {
        const coap_msg_code_t code =
            (err == -EINVAL) ? COAP_CODE_400_BAD_REQUEST :
                               COAP_CODE_500_INTERNAL_SERVER_ERROR;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_write(const uint16_t path[3], uint8_t path_len,
                     coap_message_t *p_req)
{
    uint32_t err;
    uint32_t mask;

    err = coap_message_ct_mask_get(p_req, &mask);
    if (err) {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        return;
    }

    if (mask & COAP_CT_MASK_APP_LWM2M_TLV) {
        /* Decode TLV payload */
        err = lwm2m_tlv_connectivity_monitoring_decode(&m_instance_conn_mon,
                                                        p_req->payload,
                                                        p_req->payload_len,
                                                        tlv_conn_mon_resource_decode);
    } else {
        lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_req);
        return;
    }

    if (err) {
        /* Failed to decode or to process the payload.
         * We attempted to decode a resource and failed because
         * - memory contraints or
         * - the payload contained unexpected data
         */
        const coap_msg_code_t code =
            (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                               COAP_CODE_400_BAD_REQUEST;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_discover(const uint16_t path[3], uint8_t path_len,
                        coap_message_t *p_req)
{
    uint32_t err;

    const uint16_t res = path[2];

    err = lwm2m_respond_with_instance_link(&m_instance_conn_mon.proto, res, p_req);
    if (err) {
        LWM2M_WRN("Failed to respond to discover on %s, err %d",
                  lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)), err);
    }
}

/**@brief Callback function for connectivity_monitoring instances. */
uint32_t conn_mon_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t   * p_request)
{
    uint16_t access;
    uint32_t err_code;

    const uint8_t path_len = (resource_id == LWM2M_NAMED_OBJECT) ? 2 : 3;
    const uint16_t path[] = {
        p_instance->object_id,
        p_instance->instance_id,
        resource_id
    };

    err_code = lwm2m_access_control_access_remote_get(&access,
                                                      p_instance->object_id,
                                                      p_instance->instance_id,
                                                      p_request->remote);
    if (err_code != 0) {
        return err_code;
    }

    /* Check server access */
    op_code = (access & op_code);
    if (op_code == 0) {
        lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    /* Check resource permissions */
    if (!operation_is_allowed(resource_id, op_code)) {
        LWM2M_WRN("Operation 0x%x on resource %s, not allowed", op_code,
                  lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        return 0;
    }

    if (p_instance->instance_id != 0) {
        lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_read(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE:
        on_write(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_OBSERVE:
        on_observe(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_discover(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_write_attribute(path, path_len, p_request);
        break;
    default:
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        break;
    }

    return 0;
}

static void on_object_read(coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;

    lwm2m_conn_mon_update_resource(LWM2M_NAMED_OBJECT);

    len = sizeof(buf);

    err = lwm2m_tlv_connectivity_monitoring_encode(buf + 3, &len, LWM2M_NAMED_OBJECT,
                                                   &m_instance_conn_mon);

    if (err) {
        /* TODO */
        return;
    }

    lwm2m_tlv_t tlv = {
        .id_type = TLV_TYPE_OBJECT,
        .length = len
    };

    err = lwm2m_tlv_header_encode(buf, &len, &tlv);
    if (err) {
        /* TODO */
        return;
    }

    len += tlv.length;

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_object_write_attribute(uint16_t instance, coap_message_t *p_req)
{
    int err;
    uint16_t path[] = { LWM2M_OBJ_CONN_MON };

    err = lwm2m_observer_write_attribute_handler(path, ARRAY_SIZE(path), p_req);
    if (err) {
        const coap_msg_code_t code =
            (err == -EINVAL) ? COAP_CODE_400_BAD_REQUEST :
                               COAP_CODE_500_INTERNAL_SERVER_ERROR;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_object_discover(coap_message_t * p_req)
{
    int err;

    err = lwm2m_respond_with_object_link(LWM2M_OBJ_CONN_MON, p_req);
    if (err) {
        LWM2M_WRN("Failed to discover connectivity monitoring object, err %d", err);
    }
}

/**@brief Callback function for LWM2M conn_mon objects. */
uint32_t lwm2m_conn_mon_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request)
{
    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_object_read(p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_object_write_attribute(instance_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_object_discover(p_request);
        break;
    default:
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        break;
    }

    return 0;
}


/* Fetch latest resource value */
static void lwm2m_conn_mon_update_resource(uint16_t resource_id)
{
    switch (resource_id) {
    case LWM2M_CONN_MON_NETWORK_BEARER:
        /* Values is hardcoded */
        break;
    case LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH:
    case LWM2M_CONN_MON_LINK_QUALITY:
        at_read_radio_signal_strength_and_link_quality(
            &m_instance_conn_mon.radio_signal_strength,
            &m_instance_conn_mon.link_quality);
        break;
    case LWM2M_CONN_MON_IP_ADDRESSES:
        at_read_ipaddr(&m_instance_conn_mon.ip_addresses);
        break;
    case LWM2M_CONN_MON_APN:
        if (!operator_is_vzw(true)) {
            uint8_t apn_len = 0;
            uint8_t *p_apn = lwm2m_apn_conn_prof_apn_get(lwm2m_apn_instance(), &apn_len);
            lwm2m_list_string_set(&m_instance_conn_mon.apn, 0, p_apn, apn_len);
        }
        break;
    case LWM2M_CONN_MON_CELL_ID:
        at_read_cell_id(&m_instance_conn_mon.cell_id);
        break;
    case LWM2M_CONN_MON_SMNC:
    case LWM2M_CONN_MON_SMCC:
        at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);
        break;
    case LWM2M_NAMED_OBJECT:
        at_read_radio_signal_strength_and_link_quality(
            &m_instance_conn_mon.radio_signal_strength,
            &m_instance_conn_mon.link_quality);
        at_read_cell_id(&m_instance_conn_mon.cell_id);
        at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);
        at_read_ipaddr(&m_instance_conn_mon.ip_addresses);
        if (!operator_is_vzw(true)) {
            uint8_t apn_len = 0;
            uint8_t *p_apn = lwm2m_apn_conn_prof_apn_get(lwm2m_apn_instance(), &apn_len);
            lwm2m_list_string_set(&m_instance_conn_mon.apn, 0, p_apn, apn_len);
        }
        break;
    default:
        break;
    }
}

const void * lwm2m_conn_mon_resource_reference_get(uint16_t resource_id, uint8_t *p_type)
{
    const void *p_observable = NULL;
    uint8_t type;

    switch (resource_id)
    {
        case LWM2M_CONN_MON_NETWORK_BEARER:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_conn_mon.network_bearer;
            break;
        case LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_conn_mon.radio_signal_strength;
            break;
        case LWM2M_CONN_MON_LINK_QUALITY:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_conn_mon.link_quality;
            break;
        case LWM2M_CONN_MON_CELL_ID:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_conn_mon.cell_id;
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

void lwm2m_conn_mon_init(void)
{
    //
    // Connectivity Monitoring instance.
    //
    lwm2m_instance_connectivity_monitoring_init(&m_instance_conn_mon);

    m_object_conn_mon.object_id = LWM2M_OBJ_CONN_MON;
    m_object_conn_mon.callback = lwm2m_conn_mon_object_callback;
    m_instance_conn_mon.proto.expire_time = 60; // Default to 60 second notifications.
    m_instance_conn_mon.network_bearer = 6; // LTE-FDD
    m_instance_conn_mon.available_network_bearer.len = 1;
    m_instance_conn_mon.available_network_bearer.val.p_int32[0] = 6; // LTE-FDD
    (void)at_read_radio_signal_strength_and_link_quality(&m_instance_conn_mon.radio_signal_strength, &m_instance_conn_mon.link_quality);
    m_instance_conn_mon.link_quality = 100;
    m_instance_conn_mon.link_utilization = 0;
    (void)at_read_ipaddr(&m_instance_conn_mon.ip_addresses);
    (void)at_read_cell_id(&m_instance_conn_mon.cell_id);
    (void)at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);

    m_instance_conn_mon.proto.callback = conn_mon_instance_callback;

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_mon);
}
