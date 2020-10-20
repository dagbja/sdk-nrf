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
#include <lwm2m_firmware.h>
#include <lwm2m_firmware_download.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_observer.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_remote.h>
#include <operator_check.h>
#include <lwm2m_server.h>

#include <coap_codes.h>
#include <coap_option.h>
#include <coap_observe_api.h>
#include <coap_message.h>
#include <coap_block.h>

#include <app_debug.h>
#include <dfusock.h>

static lwm2m_object_t   m_object_firmware;
static lwm2m_firmware_t m_instance_firmware;

static bool operation_is_allowed(uint16_t res, uint16_t op)
{
    if (res < ARRAY_SIZE(m_instance_firmware.operations)) {
        return m_instance_firmware.operations[res] & op;
    }

    /* Allow by default, it could be a carrier-specific resource */
    return true;
}

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

/* Callback to trigger FOTA upon receiving the Package URI */
static uint32_t tlv_write_callback(uint16_t instance_id, lwm2m_tlv_t *tlv)
{
    /* Trigger download when package URI is written to (pull-FOTA).
     * In case of push-FOTA, we decode the TLV manually and handle
     * the download somewhere else, since it is a large TLV sent
     * via multiple writes.
     */
    if (tlv->id == LWM2M_FIRMWARE_PACKAGE) {
        if (tlv->length != 0) {
            return COAP_CODE_400_BAD_REQUEST;
        }

        LWM2M_TRC("Deleting firmware...");
        return COAP_CODE_204_CHANGED;
    }

    if (tlv->id == LWM2M_FIRMWARE_PACKAGE_URI) {
        if (tlv->length >= URL_SIZE) {
            LWM2M_WRN("Package URI is too large (%d, max %d)",
                      tlv->length, URL_SIZE);
            return COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE;
        }

        if (tlv->length == 0) {
            LWM2M_TRC("Deleting firmware...");
            return COAP_CODE_204_CHANGED;
        }

        lwm2m_firmware_download_uri(m_instance_firmware.package_uri.p_val,
                                    m_instance_firmware.package_uri.len);
        return COAP_CODE_204_CHANGED;
    }

    return COAP_CODE_404_NOT_FOUND;
}

uint8_t lwm2m_firmware_state_get(uint16_t instance_id)
{
    return m_instance_firmware.state;
}

void lwm2m_firmware_state_set(uint16_t instance_id, uint8_t value)
{
#ifdef CONFIG_NRF_LWM2M_ENABLE_LOGS
    static const char * const state[] = {
        [LWM2M_FIRMWARE_STATE_IDLE] = "idle",
        [LWM2M_FIRMWARE_STATE_DOWNLOADING] = "downloading",
        [LWM2M_FIRMWARE_STATE_DOWNLOADED] = "downloaded",
        [LWM2M_FIRMWARE_STATE_UPDATING] = "updating",
    };
    LWM2M_INF("Firmware state -> %s", state[value]);
#endif

    if (m_instance_firmware.state != value)
    {
        m_instance_firmware.state  = value;
    }
}

uint8_t lwm2m_firmware_update_result_get(uint16_t instance_id)
{
    return m_instance_firmware.update_result;
}

void lwm2m_firmware_update_result_set(uint16_t instance_id, uint8_t value)
{
#ifdef CONFIG_NRF_LWM2M_ENABLE_LOGS
    static const char * const result[] = {
        [LWM2M_FIRMWARE_UPDATE_RESULT_DEFAULT] = "default",
        [LWM2M_FIRMWARE_UPDATE_RESULT_SUCCESS] = "success",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_STORAGE] = "error storage",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_MEMORY] = "error memory",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CONN_LOST] = "error conn lost",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC] = "error crc",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PKG_TYPE] = "error unsupported pkg type",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI] = "error invalid uri",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_FIRMWARE_UPDATE_FAILED] = "error firmware update failed",
        [LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PROTOCOL] = "error unsupported protocol",
    };
    LWM2M_INF("Firmware update result -> %s", result[value]);
#endif

    if (m_instance_firmware.update_result != value)
    {
        m_instance_firmware.update_result  = value;
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

static int coap_block1_get(coap_block_opt_block1_t *p_block1,
                            coap_message_t *p_req)
{
    uint32_t opt;

    for (size_t i = 0; i < p_req->options_count; i++) {
        if (p_req->options[i].number == COAP_OPT_BLOCK1) {
            coap_opt_uint_decode(&opt,
                         p_req->options[i].length,
                         p_req->options[i].data);

            coap_block_opt_block1_decode(p_block1, opt);

            LWM2M_INF("CoAP Block1: %d, sz %d, more %s",
                  p_block1->number, p_block1->size,
                  p_block1->more ? "y" : "n");

            return 0;
        }
    }

    return 1;
}

static void on_read(const uint16_t path[3], uint8_t path_len,
                    coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[128];
    size_t len;

    const uint16_t res = path[2];

    len = sizeof(buf);
    err = lwm2m_tlv_firmware_encode(buf, &len, res, &m_instance_firmware);
    if (err) {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_observe_start(const uint16_t path[3], uint8_t path_len,
                             coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[128];
    size_t len;
    coap_message_t *p_rsp;

    const uint16_t res = path[2];

     LWM2M_INF("Observe register %s",
               lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));

    len = sizeof(buf);
    err = lwm2m_tlv_firmware_encode(buf, &len, res, &m_instance_firmware);
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
}

static void on_observe_stop(const uint16_t path[3], uint8_t path_len,
                            coap_message_t *p_req)
{
    uint32_t err;

    const void * p_observable = lwm2m_observer_observable_get(path, path_len);

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
    uint32_t opt;
    uint32_t mask;
    coap_message_t *p_rsp;
    coap_block_opt_block1_t block1;

    const uint16_t res = path[2];

    err = lwm2m_coap_rsp_new(&p_rsp, p_req);
    if (err) {
        return;
    }

    err = coap_message_ct_mask_get(p_req, &mask);
    if (err) {
        lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_400_BAD_REQUEST);
        return;
    }

    if (mask & COAP_CT_MASK_APP_LWM2M_TLV) {
        err = coap_block1_get(&block1, p_req);
        if (err) {
            uint32_t coap_code;

            /* No block1 option, treat as a simple TLV write */
            coap_code = lwm2m_tlv_firmware_decode(&m_instance_firmware,
                p_req->payload, p_req->payload_len, tlv_write_callback);

            lwm2m_coap_rsp_send_with_code(p_rsp, coap_code);
            return;
        }

        /* Process fragment and fill in response code */
        lwm2m_firmware_download_inband(p_req, p_rsp, &block1);

        coap_block_opt_block1_encode(&opt, &block1);
        coap_message_opt_uint_add(p_rsp, COAP_OPT_BLOCK1, opt);

        lwm2m_coap_rsp_send(p_rsp);
        return;

    } else if ((mask & COAP_CT_MASK_PLAIN_TEXT) ||
               (mask & COAP_CT_MASK_APP_OCTET_STREAM)) {

        if (res != LWM2M_FIRMWARE_PACKAGE_URI) {
            lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_404_NOT_FOUND);
            return;
        }

        if (p_req->payload_len >= URL_SIZE) {
            LWM2M_WRN("Package URI is too large (%d, max %d)",
                      p_req->payload_len, URL_SIZE);
            lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE);
          return;
        }

        err = lwm2m_plain_text_firmware_decode(&m_instance_firmware,
                                               res, p_req->payload,
                                               p_req->payload_len);
        if (err) {
            lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE);
            return;
        }

        /* We have been able to decode the payload.
         * Any errors in the actual URI are reported via
         * the LwM2M resource by lwm2m_firmware_download.
         */
        lwm2m_firmware_download_uri(m_instance_firmware.package_uri.p_val,
                                    m_instance_firmware.package_uri.len);

        lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_204_CHANGED);
        return;
    }

    lwm2m_coap_rsp_send_with_code(p_rsp, COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT);
    return;
}

static void on_exec(uint16_t res, coap_message_t *p_req)
{
    int err;

    LWM2M_INF("Exec on /5/0/%d", res);

    if (res != LWM2M_FIRMWARE_UPDATE ) {
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_req);
    }

    /* Respond "Changed" regardless of actual result of operation;
     * any errors are reported via a dedicated LwM2M resource
     * by lwm2m_firmware_download.
     */
    err = lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
    if (err) {
        /* Continue, lwm2m_firmware_download_apply()
        * will detect and report errors
        */
    }

    err = lwm2m_firmware_download_apply();
    if (err) {
        LWM2M_ERR("Failed to schedule update, err %d", err);
        return;
    }

    /* The server (Motive) will send an observe request on /5/0/3
     * after executing /5/0/2 during in-band FOTA.
     * In any case, we should wait a bit in case we have to notify
     * /5/0/3 to let the packets be sent on air.
     */
    if (operator_is_vzw(true)) {
        lwm2m_firmware_download_reboot_schedule(SECONDS(6));
    } else {
        lwm2m_firmware_download_reboot_schedule(SECONDS(3));
    }
}

static void on_discover(const uint16_t path[3], uint8_t path_len,
                        coap_message_t *p_req)
{
    uint32_t err;

    const uint16_t res = path[2];

    err = lwm2m_respond_with_instance_link(&m_instance_firmware.proto, res, p_req);
    if (err) {
        LWM2M_WRN("Failed to respond to discover on %s, err %d",
                  lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)), err);
    }
}

/* Callback function for firmware instances. */
uint32_t firmware_instance_callback(lwm2m_instance_t *p_instance,
                                    uint16_t resource_id, uint8_t op_code,
                                    coap_message_t *p_request)
{
    uint16_t access;
    uint32_t err_code;

    const uint8_t path_len = (resource_id == LWM2M_OBJECT_INSTANCE) ? 2 : 3;
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

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);
    if (op_code == 0) {
        lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    /* Check resource permissions */
    if (!operation_is_allowed(resource_id, op_code)) {
        LWM2M_WRN("Operation 0x%x on %s, not allowed", op_code,
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
    case LWM2M_OPERATION_CODE_EXECUTE:
        on_exec(resource_id, p_request);
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

    len = sizeof(buf);

    err = lwm2m_tlv_firmware_encode(buf + 3, &len, LWM2M_OBJECT_INSTANCE,
                                    &m_instance_firmware);
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

static void on_object_write_attribute(coap_message_t *p_req)
{
    int err;
    uint16_t path[] = { LWM2M_OBJ_FIRMWARE };

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

    err = lwm2m_respond_with_object_link(LWM2M_OBJ_FIRMWARE, p_req);
    if (err) {
        LWM2M_WRN("Failed to discover firmware object, err %d", err);
    }
}

/**@brief Callback function for LWM2M firmware objects. */
uint32_t lwm2m_firmware_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request)
{
    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_object_read(p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_object_write_attribute(p_request);
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

void lwm2m_firmware_init_acl(void)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);
    uint16_t access[] = { rwde_access };
    uint16_t servers[ARRAY_SIZE(access)];
    lwm2m_list_t acl = { .len = ARRAY_SIZE(servers) };
    uint16_t owner;

    if (lwm2m_server_first_non_bootstrap_ssid_get(&owner) != 0) {
        LWM2M_WRN("Failed to find control owner");
        return;
    }

    servers[0] = owner;
    acl.val.p_uint16 = access;
    acl.p_id = servers;

    lwm2m_access_control_acl_set(LWM2M_OBJ_FIRMWARE, m_instance_firmware.proto.instance_id, &acl);
    lwm2m_access_control_owner_set(LWM2M_OBJ_FIRMWARE, m_instance_firmware.proto.instance_id, owner);
}

void lwm2m_firmware_init(void)
{
    m_object_firmware.object_id = LWM2M_OBJ_FIRMWARE;
    m_object_firmware.callback = lwm2m_firmware_object_callback;

    m_instance_firmware.proto.expire_time = 60; // Default to 60 second notifications.
    m_instance_firmware.proto.callback = firmware_instance_callback;

    lwm2m_instance_firmware_init(&m_instance_firmware);

    // Do not set the update result or the state.
    // That's set by lwm2m_firmware_download.

    // Setup default list of delivery protocols supported. For now HTTP only.
    uint8_t list[] = {
#ifdef CONFIG_COAP
        LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_COAPS,
#endif
        LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_HTTPS,
    };

    lwm2m_firmware_firmware_update_protocol_support_set(0, list, sizeof(list));

    // Setup default delivery method.
    lwm2m_firmware_firmware_delivery_method_set(0,
        LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD_PUSH_AND_PULL);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_firmware);
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
