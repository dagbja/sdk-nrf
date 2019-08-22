/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_conn_mon.h>
#include <coap_message.h>
#include <common.h>
#include <nrf_apn_class.h>
#include <lwm2m_vzw_main.h>
#include <at_interface.h>

#define VERIZON_RESOURCE 30000

static lwm2m_object_t                  m_object_conn_mon;        /**< Connectivity Monitoring base object. */
static lwm2m_connectivity_monitoring_t m_instance_conn_mon;      /**< Connectivity Monitoring object instance. */
static vzw_conn_mon_class_apn_t        m_vzw_conn_mon_class_apn; /**< Verizon specific APN names. */

static char m_apn_class_scratch_buffer[64];

// Verizon specific resources.

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
    uint16_t apn_class_len = sizeof(m_apn_class_scratch_buffer);
    int8_t apn_index = class_apn_index(apn_class);

    if (apn_index < 0 || apn_index > 3)
    {
        *p_len = 0;
        return NULL;
    }

    int retval = nrf_apn_class_read(apn_class, m_apn_class_scratch_buffer, &apn_class_len);

    if (retval == 0)
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
        (strncmp(m_vzw_conn_mon_class_apn.class_apn[apn_index].p_val, m_apn_class_scratch_buffer, len)))
    {
        // Update the network setting.
        int retval = nrf_apn_class_update(apn_class, p_value, len);

        // Update the cached value.
        if (retval == 0)
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


/**@brief Callback function for connectivity_monitoring instances. */
uint32_t conn_mon_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    LWM2M_TRC("conn_mon_instance_callback");

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

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        u32_t observe_option = 0;
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
                LWM2M_INF("Observe requested on object 4/%i/%i", p_instance->instance_id, resource_id);
                err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                    &buffer_size,
                                                                    resource_id,
                                                                    &m_instance_conn_mon);

                err_code = lwm2m_observe_register(buffer,
                                                  buffer_size,
                                                  m_instance_conn_mon.proto.expire_time,
                                                  p_request,
                                                  COAP_CT_APP_LWM2M_TLV,
                                                  (lwm2m_instance_t *)&m_instance_conn_mon);
            }
            else if (observe_option == 1) // Observe stop
            {
                LWM2M_INF("Observe cancel on object 4/%i/%i", p_instance->instance_id, resource_id);

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
        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_conn_mon_verizon_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
            switch (resource_id)
            {
                case LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH:
                    (void)at_read_radio_signal_strength(&m_instance_conn_mon.radio_signal_strength);
                    break;
                case LWM2M_CONN_MON_CELL_ID:
                    (void)at_read_cell_id(&m_instance_conn_mon.cell_id);
                    break;
                case LWM2M_CONN_MON_SMNC:
                case LWM2M_CONN_MON_SMCC:
                    (void)at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);
                    break;
                case LWM2M_NAMED_OBJECT:
                    (void)at_read_radio_signal_strength(&m_instance_conn_mon.radio_signal_strength);
                    (void)at_read_cell_id(&m_instance_conn_mon.cell_id);
                    (void)at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);
                    break;
                default:
                    break;
            }

            err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                &buffer_size,
                                                                resource_id,
                                                                &m_instance_conn_mon);
            if (err_code == ENOENT)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                return 0;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                uint32_t added_size = sizeof(buffer) - buffer_size;
                err_code = tlv_conn_mon_verizon_encode(instance_id, buffer + buffer_size, &added_size);
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
            err_code = lwm2m_tlv_connectivity_monitoring_decode(&m_instance_conn_mon,
                                                                p_request->payload,
                                                                p_request->payload_len,
                                                                tlv_conn_mon_resource_decode);
        }
        else if (mask == 0)
        {
            // TODO: Setting options should be a generic operation, not specific in conn_mon.
            //       Only using pmin and pmax for now.
            char option[255];
            for (int i = 0; i < p_request->options_count; i++) {
                memcpy(option, p_request->options[i].data, p_request->options[i].length);
                option[p_request->options[i].length] = 0;

                if (strncmp(option, "pmin=", 5) == 0) {
                    uint32_t p_min = strtol(&option[5], NULL, 10);
                    lwm2m_observable_pmin_set(p_min);
                } else if (strncmp(option, "pmax=", 5) == 0) {
                    uint32_t p_max = strtol(&option[5], NULL, 10);
                    lwm2m_observable_pmax_set(p_max);
                }
            }

            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
            return 0;
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

void lwm2m_conn_mon_observer_process(void)
{
    coap_observer_t * p_observer = NULL;
    while (coap_observe_server_next_get(&p_observer, p_observer, (coap_resource_t *)&m_instance_conn_mon) == 0)
    {
        LWM2M_TRC("Observer found");

        (void)at_read_radio_signal_strength(&m_instance_conn_mon.radio_signal_strength);

        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);
        uint32_t err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                    &buffer_size,
                                                                    LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH,
                                                                    &m_instance_conn_mon);
        if (err_code)
        {
            LWM2M_ERR("Could not encode LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH, error code: %lu", err_code);
        }

        err_code =  lwm2m_notify(buffer,
                                buffer_size,
                                p_observer,
                                COAP_TYPE_CON);
        if (err_code)
        {
            LWM2M_ERR("Could notify observer, error code: %lu", err_code);
        }
    }
}

void lwm2m_conn_mon_init(void)
{
    //
    // Connectivity Monitoring instance.
    //
    lwm2m_instance_connectivity_monitoring_init(&m_instance_conn_mon);

    m_object_conn_mon.object_id = LWM2M_OBJ_CONN_MON;
    m_instance_conn_mon.proto.expire_time = 60; // Default to 60 second notifications.
    m_instance_conn_mon.network_bearer = 6; // LTE-FDD
    m_instance_conn_mon.available_network_bearer.len = 1;
    m_instance_conn_mon.available_network_bearer.val.p_int32[0] = 6; // LTE-FDD
    (void)at_read_radio_signal_strength(&m_instance_conn_mon.radio_signal_strength);
    m_instance_conn_mon.link_quality = 100;
    m_instance_conn_mon.ip_addresses.len = 1;
    char * ip_address = "192.168.0.0";
    (void)lwm2m_bytebuffer_to_string(ip_address, strlen(ip_address), &m_instance_conn_mon.ip_addresses.val.p_string[0]);
    m_instance_conn_mon.link_utilization = 0;
    m_instance_conn_mon.apn.len = 1;
    char * apn = "VZWADMIN";
    (void)lwm2m_bytebuffer_to_string(apn, strlen(apn), &m_instance_conn_mon.apn.val.p_string[0]);
    (void)at_read_cell_id(&m_instance_conn_mon.cell_id);
    (void)at_read_smnc_smcc(&m_instance_conn_mon.smnc, &m_instance_conn_mon.smcc);

    m_instance_conn_mon.proto.callback = conn_mon_instance_callback;

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_conn_mon,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    101);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    102);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_mon);
}
