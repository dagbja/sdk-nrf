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
#include <lwm2m_firmware.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_remote.h>
#include <operator_check.h>

#include <coap_option.h>
#include <coap_observe_api.h>
#include <coap_message.h>

#include <lwm2m_common.h>
#include <lwm2m_firmware_download.h>
#include <app_debug.h>

static lwm2m_object_t   m_object_firmware;
static lwm2m_firmware_t m_instance_firmware;

char * lwm2m_firmware_package_uri_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_firmware.package_uri.len;
    return m_instance_firmware.package_uri.p_val;
}

void lwm2m_firmware_package_uri_set(uint16_t instance_id, char * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_string(p_value, len, &m_instance_firmware.package_uri) != 0)
    {
        LWM2M_ERR("Could not set package URI");
    }
}

uint8_t lwm2m_firmware_state_get(uint16_t instance_id)
{
    return m_instance_firmware.state;
}

void lwm2m_firmware_state_set(uint16_t instance_id, uint8_t value)
{
    if (m_instance_firmware.state != value)
    {
        m_instance_firmware.state  = value;
        lwm2m_firmware_notify_resource(NULL, LWM2M_FIRMWARE_STATE);
    }
}

uint8_t lwm2m_firmware_update_result_get(uint16_t instance_id)
{
    return m_instance_firmware.update_result;
}

void lwm2m_firmware_update_result_set(uint16_t instance_id, uint8_t value)
{
    if (m_instance_firmware.update_result != value)
    {
        m_instance_firmware.update_result  = value;
        lwm2m_firmware_notify_resource(NULL, LWM2M_FIRMWARE_UPDATE_RESULT);
    }
}

uint8_t * lwm2m_firmware_firmware_update_protocol_support_get(uint16_t instance_id, uint8_t * p_len)
{
    *p_len = m_instance_firmware.firmware_update_protocol_support.len;
    return m_instance_firmware.firmware_update_protocol_support.val.p_uint8;
}

void lwm2m_firmware_firmware_update_protocol_support_set(uint16_t instance_id, uint8_t * p_value, uint8_t len)
{
    if (lwm2m_bytebuffer_to_list(p_value, len, &m_instance_firmware.firmware_update_protocol_support) != 0)
    {
        LWM2M_ERR("Could not set update protocol support");
    }
}

uint8_t lwm2m_firmware_firmware_delivery_method_get(uint16_t instance_id)
{
    return m_instance_firmware.firmware_update_delivery_method;
}

void lwm2m_firmware_firmware_delivery_method_set(uint16_t instance_id, uint8_t value)
{
    m_instance_firmware.firmware_update_delivery_method  = value;
}

lwm2m_firmware_t * lwm2m_firmware_get_instance(uint16_t instance_id)
{
    return &m_instance_firmware;
}

lwm2m_object_t * lwm2m_firmware_get_object(void)
{
    return &m_object_firmware;
}

/**@brief Callback function for device instances. */
uint32_t firmware_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t   * p_request)
{
    LWM2M_TRC("firmware_instance_callback");

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
                    // case LWM2M_FIRMWARE_PACKAGE_URI:
                    case LWM2M_FIRMWARE_STATE:
                    case LWM2M_FIRMWARE_UPDATE_RESULT:
                    // case LWM2M_FIRMWARE_PKG_NAME:
                    // case LWM2M_FIRMWARE_PKG_VERSION:
                    // case LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT:
                    // case LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD:
                    {
                        coap_message_t *p_message;

                        LWM2M_INF("Observe requested on resource %s", lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
                        err_code = lwm2m_tlv_firmware_encode(buffer,
                                                             &buffer_size,
                                                             resource_id,
                                                             &m_instance_firmware);
                        if (err_code != 0)
                        {
                            LWM2M_INF("Failed to perform the TLV encoding");
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

                        err_code = lwm2m_coap_message_send_to_remote(p_message, p_request->remote, buffer, buffer_size);
                        if (err_code != 0)
                        {
                            LWM2M_INF("Failed to respond to Observe request");
                            lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                            return err_code;
                        }

                        lwm2m_observable_metadata_init(p_request->remote, path, path_len);

                        break;
                    }

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
                    const void * p_observable = lwm2m_observable_reference_get(path, path_len);
                    lwm2m_observe_unregister(p_request->remote, p_observable);
                    lwm2m_notif_attr_storage_update(path, path_len, p_request->remote);
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
        err_code = lwm2m_tlv_firmware_encode(buffer,
                                             &buffer_size,
                                             resource_id,
                                             &m_instance_firmware);

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
            lwm2m_firmware_t unpack_struct;
            memset(&unpack_struct, 0, sizeof(lwm2m_firmware_t));

            err_code = lwm2m_tlv_firmware_decode(&unpack_struct,
                                                 p_request->payload,
                                                 p_request->payload_len,
                                                 NULL);

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                // Pass. Write the whole object instance.
            }
            else
            {
                switch (resource_id)
                {
                    case LWM2M_FIRMWARE_PACKAGE:
                    {
                        (void)lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
                        break;
                    }

                    case LWM2M_FIRMWARE_PACKAGE_URI:
                    {
                        int err;

                        lwm2m_firmware_package_uri_set(instance_id,
                            unpack_struct.package_uri.p_val, unpack_struct.package_uri.len);

                        err = lwm2m_firmware_download_uri(
                            m_instance_firmware.package_uri.p_val,
                            m_instance_firmware.package_uri.len);

                        if (err) {
                            LWM2M_ERR("Invalid protocol in package URI");
                        }
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
            err_code = lwm2m_plain_text_firmware_decode(&m_instance_firmware,
                                                        resource_id,
                                                        p_request->payload,
                                                        p_request->payload_len);

            switch (err_code)
            {
                case EINVAL:
                    (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                    return err_code;

                case ENOTSUP:
                    (void)lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
                    return err_code;
            }

            __ASSERT_NO_MSG(resource_id == LWM2M_FIRMWARE_PACKAGE_URI);

            int err;

            err = lwm2m_firmware_download_uri(
                m_instance_firmware.package_uri.p_val,
                m_instance_firmware.package_uri.len);

            if (err) {
                lwm2m_firmware_update_result_set(0,
                    LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI);
            }
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
    else if (op_code == LWM2M_OPERATION_CODE_WRITE_ATTR)
    {
        err_code = lwm2m_write_attribute_handler(path, path_len, p_request);

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
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
            case LWM2M_FIRMWARE_UPDATE:
            {
                int err;

                err = lwm2m_firmware_download_apply();
                if (err)
                {
                    break;
                }

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                lwm2m_firmware_download_reboot_schedule();

                break;
            }

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

const void * lwm2m_firmware_resource_reference_get(uint16_t resource_id, uint8_t *p_type)
{
    const void *p_observable = NULL;
    uint8_t type;

    switch (resource_id)
    {
        case LWM2M_FIRMWARE_STATE:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_firmware.state;
            break;
        case LWM2M_FIRMWARE_UPDATE_RESULT:
            type = LWM2M_OBSERVABLE_TYPE_INT;
            p_observable = &m_instance_firmware.update_result;
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

void lwm2m_firmware_notify_resource(struct nrf_sockaddr * p_remote_server, uint16_t resource_id)
{
    uint16_t short_server_id;
    coap_observer_t * p_observer = NULL;
    const void *p_observable = lwm2m_firmware_resource_reference_get(resource_id, NULL);

    while (coap_observe_server_next_get(&p_observer, p_observer,
               (void *)&m_instance_firmware.resource_ids[resource_id]) == 0)
    {
        lwm2m_remote_short_server_id_find(&short_server_id, p_observer->remote);
        if (lwm2m_remote_reconnecting_get(short_server_id)) {
            /* Wait for reconnection */
            continue;
        }

        if (p_remote_server != NULL) {
            /* Only notify to given remote */
            if (memcmp(p_observer->remote, p_remote_server,
                       sizeof(struct nrf_sockaddr)) != 0) {
                continue;
            }
        }

        uint32_t err_code;
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        LWM2M_TRC("Observer found");
        err_code = lwm2m_tlv_firmware_encode(buffer,
                                             &buffer_size,
                                             resource_id,
                                             &m_instance_firmware);
        if (err_code)
        {
            LWM2M_ERR("Could not encode resource_id %u, error code: %lu",
                      resource_id, err_code);
        }

        coap_msg_type_t type = (lwm2m_observer_notification_is_con(p_observable, short_server_id)) ? COAP_TYPE_CON : COAP_TYPE_NON;

        LWM2M_INF("Notify /5/0/%d", resource_id);
        err_code =  lwm2m_notify(buffer,
                                 buffer_size,
                                 p_observer,
                                 type);
        if (err_code)
        {
            LWM2M_INF("Notify /5/0/%d failed: %s (%ld), %s (%d)", resource_id,
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());

            lwm2m_request_remote_reconnect(p_observer->remote);
        }
    }
}

/**@brief Callback function for LWM2M firmware objects. */
uint32_t lwm2m_firmware_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request)
{
    LWM2M_TRC("firmware_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t buffer_len = 255;
        uint8_t  buffer[buffer_len];

        err_code = lwm2m_tlv_firmware_encode(buffer + 3, &buffer_len,
                                             LWM2M_NAMED_OBJECT,
                                             &m_instance_firmware);
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
    else if (op_code == LWM2M_OPERATION_CODE_WRITE_ATTR)
    {
        uint16_t path[] = { p_object->object_id };
        uint8_t path_len = ARRAY_SIZE(path);

        err_code = lwm2m_write_attribute_handler(path, path_len, p_request);

        if (err_code == 0)
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

    return err_code;
}

void lwm2m_firmware_init_acl(void)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    lwm2m_instance_acl_t acl = { 0 };

    if (operator_is_vzw(true))
    {
        acl.access[0] = rwde_access;
        acl.server[0] = 102;
        acl.owner = 102;
    }
    else if (operator_is_att(true))
    {
        acl.access[0] = rwde_access;
        acl.server[0] = 1;
        acl.owner = 1;
    }
    else
    {
        // TODO: Remove when fixing ACL
        acl.access[0] = rwde_access;
        acl.server[0] = 123;
        acl.owner = 123;
    }

    lwm2m_set_instance_acl((lwm2m_instance_t *)&m_instance_firmware, LWM2M_PERMISSION_READ, &acl);
}

void lwm2m_firmware_init(void)
{
    m_object_firmware.object_id = LWM2M_OBJ_FIRMWARE;
    m_object_firmware.callback = lwm2m_firmware_object_callback;

    m_instance_firmware.proto.expire_time = 60; // Default to 60 second notifications.
    m_instance_firmware.proto.callback = firmware_instance_callback;

    lwm2m_instance_firmware_init(&m_instance_firmware);

    // Setup of package download state.
    lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_IDLE);

    // Setup of update result status.
    lwm2m_firmware_update_result_set(0, LWM2M_FIRMWARE_UPDATE_RESULT_DEFAULT);

    // Setup default list of delivery protocols supported. For now HTTP only.
    uint8_t list[] = {LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_HTTPS};
    lwm2m_firmware_firmware_update_protocol_support_set(0, list, sizeof(list));

    // Setup default delivery method.
    lwm2m_firmware_firmware_delivery_method_set(0, LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD_PULL_ONLY);

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_firmware,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    lwm2m_firmware_init_acl();

    uint32_t errcode = lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_firmware);
    if (errcode == ENOMEM)
    {
        LWM2M_ERR("No more space for firmware object to be added.");
    }
}
