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
#include <lwm2m_portfolio.h>
#include <coap_message.h>
#include <lwm2m_common.h>
#include <lwm2m_carrier_main.h>
#include <at_interface.h>

#define HOST_DEVICE_ID_0           "HUID0"
#define HOST_DEVICE_MANUFACTURER_0 "HMAN0"
#define HOST_DEVICE_MODEL_0        "HMOD0"
#define HOST_DEVICE_SW_VERSION_0   "HSW0"

#define HOST_DEVICE_ID_1           "HUID1"
#define HOST_DEVICE_MANUFACTURER_1 "HMAN1"
#define HOST_DEVICE_MODEL_1        "HMOD1"
#define HOST_DEVICE_SW_VERSION_1   "HSW1"

static lwm2m_object_t    m_object_portfolio;          /**< Portfolio base object. */
static lwm2m_portfolio_t m_instance_portfolio[2];     /**< Portfolio object instance. */
static char *            m_portfolio_identity_val[][LWM2M_PORTFOLIO_IDENTITY_INSTANCES] =
{
    { HOST_DEVICE_ID_0, HOST_DEVICE_MANUFACTURER_0, HOST_DEVICE_MODEL_0, HOST_DEVICE_SW_VERSION_0 },
    { HOST_DEVICE_ID_1, HOST_DEVICE_MANUFACTURER_1, HOST_DEVICE_MODEL_1, HOST_DEVICE_SW_VERSION_1 }
};
static lwm2m_string_t    m_portfolio_identity[ARRAY_SIZE(m_instance_portfolio)][LWM2M_PORTFOLIO_IDENTITY_INSTANCES];

// LWM2M core resources.

lwm2m_portfolio_t * lwm2m_portfolio_get_instance(uint16_t instance_id)
{
    return &m_instance_portfolio[instance_id];
}

lwm2m_object_t * lwm2m_portfolio_get_object(void)
{
    return &m_object_portfolio;
}

/**@brief Callback function for portfolio instances. */
uint32_t portfolio_instance_callback(lwm2m_instance_t * p_instance,
                                     uint16_t           resource_id,
                                     uint8_t            op_code,
                                     coap_message_t *   p_request)
{
    LWM2M_TRC("portfolio_instance_callback");

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

    if (instance_id >= ARRAY_SIZE(m_instance_portfolio))
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {

        err_code = lwm2m_tlv_portfolio_encode(buffer,
                                              &buffer_size,
                                              resource_id,
                                              &m_instance_portfolio[instance_id]);
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
            err_code = lwm2m_tlv_portfolio_decode(&m_instance_portfolio[instance_id],
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

/**@brief Callback function for LWM2M portfolio objects. */
uint32_t lwm2m_portfolio_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request)
{
    LWM2M_TRC("portfolio_object_callback");

    uint32_t err_code = 0;
    uint8_t  buffer[300];
    uint32_t buffer_len      = sizeof(buffer);
    uint32_t buffer_max_size = buffer_len;
    const uint16_t path[] = { LWM2M_OBJ_PORTFOLIO };
    uint8_t  path_len = ARRAY_SIZE(path);

    if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        uint32_t observe_option = 0;

        if (instance_id != LWM2M_INVALID_INSTANCE)
        {
            lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return 0;
        }

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
                coap_message_t *p_message;

                LWM2M_INF("Observe requested on object /16");
                err_code = lwm2m_tlv_element_encode(buffer, &buffer_len, path, path_len);
                if (err_code != 0)
                {
                    LWM2M_INF("Failed to perform the TLV encoding, err %d", err_code);
                    lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                    return err_code;
                }

                err_code = lwm2m_observe_register(path, path_len, p_request, &p_message);
                if (err_code != 0)
                {
                    LWM2M_INF("Failed to register the observer");
                    lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                    return err_code;
                }

                err_code = lwm2m_coap_message_send_to_remote(p_message, p_request->remote, buffer, buffer_len);
                if (err_code != 0)
                {
                    LWM2M_INF("Failed to respond to Observe request");
                    lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                    return err_code;
                }

                lwm2m_observable_metadata_init(p_request->remote, path, path_len);
            }
            else if (observe_option == 1) // Observe stop
            {
                LWM2M_INF("Observe cancel on object /16");
                const void * p_observable = lwm2m_observable_reference_get(path, path_len);
                lwm2m_observe_unregister(p_request->remote, p_observable);
                lwm2m_notif_attr_storage_update(path, path_len, p_request->remote);

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
        uint32_t index = 0;
        uint8_t  instance_buffer[150];
        uint32_t instance_buffer_len = sizeof(instance_buffer);

        for (int i = 0; i < ARRAY_SIZE(m_instance_portfolio); i++)
        {
            uint16_t access = 0;
            lwm2m_instance_t * p_instance = (lwm2m_instance_t *)&m_instance_portfolio[i];
            uint32_t err_code = lwm2m_access_remote_get(&access,
                                                        p_instance,
                                                        p_request->remote);
            if (err_code != 0 || (access & op_code) == 0)
            {
                continue;
            }

            instance_buffer_len = 150;
            err_code = lwm2m_tlv_portfolio_encode(instance_buffer,
                                                  &instance_buffer_len,
                                                  LWM2M_NAMED_OBJECT,
                                                  &m_instance_portfolio[i]);
            if (err_code != 0)
            {
                // ENOMEM should not happen. Then it is a bug.
                break;
            }

            lwm2m_tlv_t tlv = {
                .id_type = TLV_TYPE_OBJECT,
                .id = i,
                .length = instance_buffer_len
            };
            err_code = lwm2m_tlv_header_encode(buffer + index, &buffer_len, &tlv);

            index += buffer_len;
            buffer_len = buffer_max_size - index;

            memcpy(buffer + index, instance_buffer, instance_buffer_len);

            index += instance_buffer_len;
            buffer_len = buffer_max_size - index;
        }

        err_code = lwm2m_respond_with_payload(buffer, index, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_DISCOVER)
    {
        err_code = lwm2m_respond_with_object_link(p_object->object_id, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE_ATTR)
    {
        err_code = lwm2m_write_attribute_handler(path, path_len, p_request);
        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
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

void lwm2m_portfolio_init_acl(void)
{
    for (int i = 0; i < ARRAY_SIZE(m_instance_portfolio); i++)
    {
        lwm2m_set_carrier_acl((lwm2m_instance_t *)&m_instance_portfolio[i]);
    }
}

void lwm2m_portfolio_init(void)
{
    //
    // Portfolio instance.
    //
    m_object_portfolio.object_id = LWM2M_OBJ_PORTFOLIO;
    m_object_portfolio.callback = lwm2m_portfolio_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < ARRAY_SIZE(m_instance_portfolio); i++)
    {
        lwm2m_instance_portfolio_init(&m_instance_portfolio[i]);
        m_instance_portfolio[i].proto.instance_id = i;
        m_instance_portfolio[i].proto.callback = portfolio_instance_callback;

        for (int j = 0; j < LWM2M_PORTFOLIO_IDENTITY_INSTANCES; j++)
        {
            (void)lwm2m_bytebuffer_to_string(m_portfolio_identity_val[i][j], strlen(m_portfolio_identity_val[i][j]), &m_portfolio_identity[i][j]);
        }
        m_instance_portfolio[i].identity.val.p_string = m_portfolio_identity[i];
        m_instance_portfolio[i].identity.len = ARRAY_SIZE(m_portfolio_identity_val[i]);

        // Set bootstrap server as owner.
        (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_portfolio[i],
                                        LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_portfolio[i]);
    }

    // Initialize ACL
    lwm2m_portfolio_init_acl();
}
