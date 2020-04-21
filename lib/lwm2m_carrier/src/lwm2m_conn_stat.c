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
#include <lwm2m_observer.h>
#include <lwm2m_conn_stat.h>
#include <coap_message.h>
#include <lwm2m_carrier_main.h>
#include <at_interface.h>

/* Collection Period timer */
static void *collection_period_timer;

static lwm2m_object_t                  m_object_conn_stat;        /**< Connectivity Statistics base object. */
static lwm2m_connectivity_statistics_t m_instance_conn_stat;      /**< Connectivity Statistics object instance. */

// LWM2M core resources.

lwm2m_connectivity_statistics_t * lwm2m_conn_stat_get_instance(uint16_t instance_id)
{
    return &m_instance_conn_stat;
}

lwm2m_object_t * lwm2m_conn_stat_get_object(void)
{
    return &m_object_conn_stat;
}

static void lwm2m_conn_stat_collection_period(void *timer)
{
    ARG_UNUSED(timer);

    at_stop_connstat();
}


/**@brief Callback function for connectivity_statistics instances. */
uint32_t conn_stat_instance_callback(lwm2m_instance_t * p_instance,
                                     uint16_t           resource_id,
                                     uint8_t            op_code,
                                     coap_message_t *   p_request)
{
    LWM2M_TRC("conn_stat_instance_callback");

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
                    // case LWM2M_CONN_STAT_SMS_TX_COUNTER:
                    // case LWM2M_CONN_STAT_SMS_RX_COUNTER:
                    // case LWM2M_CONN_STAT_TX_DATA:
                    // case LWM2M_CONN_STAT_RX_DATA:
                    // case LWM2M_CONN_STAT_MAX_MSG_SIZE:
                    // case LWM2M_CONN_STAT_AVG_MSG_SIZE:
                    // case LWM2M_CONN_STAT_COLLECTION_PERIOD:
                    /*
                    {
                        LWM2M_INF("Observe requested on resource /7/%i/%i", p_instance->instance_id, resource_id);
                        err_code = lwm2m_tlv_conn_stat_encode(buffer,
                                                              &buffer_size,
                                                              resource_id,
                                                              &m_instance_conn_stat);

                        err_code = lwm2m_observe_register(buffer,
                                                        buffer_size,
                                                        m_instance_conn_stat.proto.expire_time,
                                                        p_request,
                                                        COAP_CT_APP_LWM2M_TLV,
                                                        (void *)&m_instance_conn_stat.resource_ids[resource_id]);

                        break;
                    }
                    */

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
                    LWM2M_INF("Observe cancel on instance %s, no match", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
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
        if (at_read_connstat(&m_instance_conn_stat) != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
            return 0;

        }

        err_code = lwm2m_tlv_connectivity_statistics_encode(buffer,
                                                            &buffer_size,
                                                            resource_id,
                                                            &m_instance_conn_stat);
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
            err_code = lwm2m_tlv_connectivity_statistics_decode(&m_instance_conn_stat,
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
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_CONN_STAT_START:
            {
                (void)at_start_connstat();

                if (m_instance_conn_stat.collection_period > 0)
                {
                    lwm2m_os_timer_start(collection_period_timer,
                                         SECONDS(m_instance_conn_stat.collection_period));
                }

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
            }
            break;

            case LWM2M_CONN_STAT_STOP:
            {
                (void)at_stop_connstat();
                lwm2m_os_timer_release(collection_period_timer);

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
            }
            break;

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

/**@brief Callback function for LWM2M conn_stat objects. */
uint32_t lwm2m_conn_stat_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request)
{
    LWM2M_TRC("conn_stat_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t buffer_len = 255;
        uint8_t  buffer[buffer_len];

        err_code = lwm2m_tlv_connectivity_statistics_encode(buffer + 3, &buffer_len,
                                                            LWM2M_NAMED_OBJECT,
                                                            &m_instance_conn_stat);
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

void lwm2m_conn_stat_init(void)
{
    //
    // Connectivity Statistics instance.
    //
    lwm2m_instance_connectivity_statistics_init(&m_instance_conn_stat);

    m_object_conn_stat.object_id = LWM2M_OBJ_CONN_STAT;
    m_object_conn_stat.callback = lwm2m_conn_stat_object_callback;

    m_instance_conn_stat.sms_tx_counter = 0;
    m_instance_conn_stat.sms_rx_counter = 0;
    m_instance_conn_stat.tx_data = 0;
    m_instance_conn_stat.rx_data = 0;
    m_instance_conn_stat.max_message_size = 0;
    m_instance_conn_stat.average_message_size = 0;
    m_instance_conn_stat.collection_period = 0;

    m_instance_conn_stat.proto.callback = conn_stat_instance_callback;

    // Collection period timer.
    collection_period_timer = lwm2m_os_timer_get(lwm2m_conn_stat_collection_period);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_stat);
}
