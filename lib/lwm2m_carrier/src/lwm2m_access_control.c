/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_access_control.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_access_control.h>
#include <coap_message.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_remote.h>
#include <operator_check.h>
#include <lwm2m_firmware.h>
#include <lwm2m_carrier.h>
#include <lwm2m_factory_bootstrap.h>
#include <lwm2m_server.h>

#define LWM2M_ACL_INTERNAL_NOT_FOUND 65537
#define LWM2M_ACL_NO_PERM            0
#define LWM2M_ACL_FULL_PERM          (LWM2M_PERMISSION_READ    | \
                                      LWM2M_PERMISSION_WRITE   | \
                                      LWM2M_PERMISSION_EXECUTE | \
                                      LWM2M_PERMISSION_DELETE  | \
                                      LWM2M_PERMISSION_CREATE)

static lwm2m_object_t         m_object_acc_control;                                       /**< Access Control base object. */
static lwm2m_access_control_t m_instance_acc_control[LWM2M_ACCESS_CONTROL_MAX_INSTANCES]; /**< Access Control object instance. */

lwm2m_access_control_t * lwm2m_access_control_get_instance(uint16_t instance_id)
{
    lwm2m_access_control_t *p_instance = NULL;

    if (!lwm2m_ctx_access_control_enable_status_get())
    {
        return NULL;
    }

    for (int i = 0; i < ARRAY_SIZE(m_instance_acc_control); i++)
    {
        if (m_instance_acc_control[i].proto.instance_id == instance_id)
        {
            p_instance = &m_instance_acc_control[i];
            break;
        }
    }

    return p_instance;
}

lwm2m_object_t * lwm2m_access_control_get_object(void)
{
    return &m_object_acc_control;
}

static lwm2m_access_control_t * lwm2m_access_control_get_new_instance(void)
{
    lwm2m_access_control_t *p_instance = NULL;

    for (int i = 0; i < ARRAY_SIZE(m_instance_acc_control); i++)
    {
        if (m_instance_acc_control[i].object_id == LWM2M_INVALID_INSTANCE)
        {
            p_instance = &m_instance_acc_control[i];
            break;
        }
    }

    return p_instance;
}

uint32_t lwm2m_access_control_find(uint16_t object_id, uint16_t instance_id, uint16_t *p_access_control_id)
{
    if (!p_access_control_id)
    {
        return EINVAL;
    }

    for (int i = 0; i < ARRAY_SIZE(m_instance_acc_control); i++)
    {
        if (m_instance_acc_control[i].object_id == object_id &&
            m_instance_acc_control[i].instance_id == instance_id)
        {
            *p_access_control_id = i;
            return 0;
        }
    }

    return ENOENT;
}

uint32_t lwm2m_access_control_instance_bind(uint16_t object_id, uint16_t instance_id, uint16_t *p_access_control_id)
{
    lwm2m_access_control_t *p_access_control;
    uint32_t err_code;
    uint16_t inst;

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code == 0)
    {
        // Access Control instance already binded to the instance.
        if (p_access_control_id)
        {
            *p_access_control_id = inst;
        }

        return 0;
    }

    p_access_control = lwm2m_access_control_get_new_instance();

    if (!p_access_control)
    {
        LWM2M_WRN("Insufficient Access Control object instances");
        return ENOMEM;
    }

    p_access_control->object_id = object_id;
    p_access_control->instance_id = instance_id;

    if (p_access_control_id)
    {
        *p_access_control_id = p_access_control->proto.instance_id;
    }

    return lwm2m_coap_handler_instance_add((lwm2m_instance_t *)p_access_control);
}

static void lwm2m_access_control_instance_delete(uint16_t instance_id)
{
    if (instance_id >= ARRAY_SIZE(m_instance_acc_control))
    {
        return;
    }

    // Unbind the instance.
    m_instance_acc_control[instance_id].object_id = LWM2M_INVALID_INSTANCE;
    m_instance_acc_control[instance_id].instance_id = LWM2M_INVALID_INSTANCE;
    m_instance_acc_control[instance_id].control_owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;

    // Reset the ACLs.
    memset(m_instance_acc_control[instance_id].acl.val.p_uint16, 0, m_instance_acc_control[instance_id].acl.max_len);
    memset(m_instance_acc_control[instance_id].acl.p_id, 0, m_instance_acc_control[instance_id].acl.max_len);
    m_instance_acc_control[instance_id].acl.len = 0;

    // Remove the instance from the request handler.
    lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_acc_control[instance_id]);
}

void lwm2m_access_control_delete_instances(void)
{
    for (int i = 0; i < ARRAY_SIZE(m_instance_acc_control); i++)
    {
        lwm2m_access_control_instance_delete(i);
    }
}

void lwm2m_access_control_instance_unbind(uint16_t object_id, uint16_t instance_id)
{
    uint32_t err_code;
    uint16_t inst;

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code != 0)
    {
        // No Access Control instance binded, nothing to do.
        return;
    }

    lwm2m_access_control_instance_delete(inst);
}

void lwm2m_access_control_acl_set(uint16_t             object_id,
                                  uint16_t             instance_id,
                                  const lwm2m_list_t * p_acl)
{
    uint32_t err_code;
    uint16_t inst;

    if (!p_acl || p_acl->len > LWM2M_MAX_SERVERS)
    {
        LWM2M_WRN("ACL to be set is invalid");
        return;
    }

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code != 0)
    {
        err_code = lwm2m_access_control_instance_bind(object_id, instance_id, &inst);

        if (err_code != 0)
        {
            return;
        }
    }

    for (uint32_t i = 0; i < p_acl->len; i++)
    {
        // Set server access.
        m_instance_acc_control[inst].acl.p_id[i] = p_acl->p_id[i];
        m_instance_acc_control[inst].acl.val.p_uint16[i] = p_acl->val.p_uint16[i];
    }

    m_instance_acc_control[inst].acl.len = p_acl->len;
}

void lwm2m_access_control_owner_set(uint16_t object_id, uint16_t instance_id, uint16_t owner)
{
    uint16_t inst;
    uint32_t err_code;

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code != 0)
    {
        LWM2M_WRN("Failed to find matching Access Control instance");
        return;
    }

    m_instance_acc_control[inst].control_owner = owner;
}

void lwm2m_access_control_carrier_acl_set(uint16_t object_id, uint16_t instance_id)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    lwm2m_list_t acl;
    uint16_t access[LWM2M_MAX_SERVERS];
    uint16_t servers[LWM2M_MAX_SERVERS];
    uint16_t owner;

    if (lwm2m_server_first_non_bootstrap_ssid_get(&owner) != 0) {
        LWM2M_WRN("Failed to find control owner");
        return;
    }

    if (operator_is_vzw(true))
    {
        access[0] = rwde_access;
        servers[0] = 101;
        access[1] = rwde_access;
        servers[1] = 102;
        access[2] = rwde_access;
        servers[2] = 1000;
        acl.len = 3;
    }
    else
    {
        access[0] = rwde_access;
        servers[0] = owner;
        acl.len = 1;
    }

    acl.val.p_uint16 = access;
    acl.p_id = servers;

    lwm2m_access_control_acl_set(object_id, instance_id, &acl);
    lwm2m_access_control_owner_set(object_id, instance_id, owner);
}

static uint32_t index_find(const lwm2m_list_t * p_list, uint16_t short_server_id)
{
    for (int i = 0; i < p_list->len; ++i)
    {
        if (p_list->p_id[i] == short_server_id)
        {
            return i;
        }
    }

    return LWM2M_ACL_INTERNAL_NOT_FOUND;
}

uint32_t lwm2m_access_control_acl_check(uint16_t * p_access,
                                        uint16_t   object_id,
                                        uint16_t   instance_id,
                                        uint16_t   short_server_id)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    uint32_t index;
    uint32_t err_code;
    uint16_t inst;

    if (!p_access)
    {
        return EINVAL;
    }

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code != 0)
    {
        return err_code;
    }

    // Owner has full access
    if (short_server_id == m_instance_acc_control[inst].control_owner)
    {
        *p_access = LWM2M_ACL_FULL_PERM;

        LWM2M_TRC("%u is owner", short_server_id);

        return 0;
    }

    index = index_find(&m_instance_acc_control[inst].acl, short_server_id);

    if (index == LWM2M_ACL_INTERNAL_NOT_FOUND)
    {
        // Set access to LWM2M_ACL_NO_PERM in case of no error checking.
        *p_access = LWM2M_ACL_NO_PERM;

        LWM2M_TRC("%u was not found", short_server_id);

        return ENOENT;
    }

    *p_access = m_instance_acc_control[inst].acl.val.p_uint16[index];

    LWM2M_TRC("Success");

    return 0;
}

uint32_t lwm2m_access_control_access_remote_get(uint16_t            * p_access,
                                                uint16_t              object_id,
                                                uint16_t              instance_id,
                                                struct nrf_sockaddr * p_remote)
{
    uint16_t short_server_id;
    uint16_t inst;
    uint32_t err_code = 0;

    if (!p_access || !p_remote)
    {
        return EINVAL;
    }

    if (!lwm2m_ctx_access_control_enable_status_get())
    {
        /* Give full access if in Access Control-disabled context. */
        *p_access = LWM2M_ACL_FULL_PERM | LWM2M_OPERATION_CODE_DISCOVER | LWM2M_OPERATION_CODE_OBSERVE | LWM2M_OPERATION_CODE_WRITE_ATTR;
        return 0;
    }

    err_code = lwm2m_access_control_find(object_id, instance_id, &inst);

    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_remote);

    if (err_code == 0)
    {
        err_code = lwm2m_access_control_acl_check(p_access, object_id, instance_id, short_server_id);
        // If we can't find the permission we return defaults.
        if (err_code != 0)
        {
            err_code = lwm2m_access_control_acl_check(p_access,
                                                      object_id,
                                                      instance_id,
                                                      LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);
        }
    }

    if ((*p_access & LWM2M_PERMISSION_READ) > 0)
    {
        // Observe and discover is allowed if READ is allowed.
        *p_access = (*p_access | LWM2M_OPERATION_CODE_DISCOVER | LWM2M_OPERATION_CODE_OBSERVE | LWM2M_OPERATION_CODE_WRITE_ATTR);
    }

    return err_code;
}

static void on_read(const uint16_t path[3], uint8_t path_len,
                    coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[100];
    size_t len;
    lwm2m_access_control_t *p_instance;

    const uint16_t inst = path[1];
    const uint16_t res = path[2];

    p_instance = lwm2m_access_control_get_instance(inst);

    len = sizeof(buf);
    err = lwm2m_tlv_access_control_encode(buf, &len, res, p_instance);
    if (err) {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_write(const uint16_t path[3], uint8_t path_len,
                     coap_message_t *p_req)
{
    uint16_t short_server_id;
    uint32_t err;
    uint32_t mask;
    lwm2m_access_control_t *p_instance;

    const uint16_t inst = path[1];

    p_instance = lwm2m_access_control_get_instance(inst);

    err = lwm2m_remote_short_server_id_find(&short_server_id, p_req->remote);
    if (err) {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        return;
    }

    if (short_server_id != p_instance->control_owner &&
        short_server_id != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID) {
        lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_req);
        return;
    }

    err = coap_message_ct_mask_get(p_req, &mask);
    if (err) {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        return;
    }

    if (mask & COAP_CT_MASK_APP_LWM2M_TLV) {
        /* Decode TLV payload */
        err = lwm2m_tlv_access_control_decode(p_instance,
                                              p_req->payload,
                                              p_req->payload_len,
                                              NULL);
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

    lwm2m_storage_access_control_store();

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_discover(const uint16_t path[3], uint8_t path_len,
                        coap_message_t *p_req)
{
    uint32_t err;
    lwm2m_access_control_t *p_instance;

    const uint16_t inst = path[1];
    const uint16_t res = path[2];

    p_instance = lwm2m_access_control_get_instance(inst);

    err = lwm2m_respond_with_instance_link(&p_instance->proto, res, p_req);
    if (err) {
        LWM2M_WRN("Failed to respond to discover on %s, err %d",
            lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)), err);
    }
}

static bool operation_is_allowed(uint16_t inst, uint16_t res, uint16_t op)
{
    lwm2m_access_control_t *p_instance = lwm2m_access_control_get_instance(inst);

    if (!p_instance)
    {
        return false;
    }

    if (res < ARRAY_SIZE(p_instance->operations)) {
        return p_instance->operations[res] & op;
    }

    /* Allow by default, it could be a carrier-specific resource */
    return true;
}

/**@brief Callback function for access_control instances. */
uint32_t access_control_instance_callback(lwm2m_instance_t * p_instance,
                                          uint16_t           resource_id,
                                          uint8_t            op_code,
                                          coap_message_t *   p_request)
{
    const uint8_t path_len = (resource_id == LWM2M_NAMED_OBJECT) ? 2 : 3;
    const uint16_t path[] = {
        p_instance->object_id,
        p_instance->instance_id,
        resource_id
    };

    /* Check resource permissions */
    if (!operation_is_allowed(path[1], path[2], op_code)) {
        LWM2M_WRN("Operation 0x%x on %s, not allowed", op_code,
                  lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        return 0;
    }

    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_read(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE:
        on_write(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_discover(path, path_len, p_request);
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
    uint8_t buf[300];
    size_t len;

    const uint16_t path[] = { LWM2M_OBJ_ACCESS_CONTROL };

    len = sizeof(buf);

    err = lwm2m_tlv_element_encode(buf, &len, path, ARRAY_SIZE(path));
    if (err) {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_object_discover(coap_message_t * p_req)
{
    int err;

    err = lwm2m_respond_with_object_link(LWM2M_OBJ_ACCESS_CONTROL, p_req);
    if (err) {
        LWM2M_WRN("Failed to discover access control object, err %d", err);
    }
}

/**@brief Callback function for LWM2M access_control objects. */
uint32_t lwm2m_access_control_object_callback(lwm2m_object_t * p_object,
                                              uint16_t         instance_id,
                                              uint8_t          op_code,
                                              coap_message_t * p_request)
{
    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_object_read(p_request);
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

void lwm2m_access_control_init(void)
{
    m_object_acc_control.object_id = LWM2M_OBJ_ACCESS_CONTROL;
    m_object_acc_control.callback = lwm2m_access_control_object_callback;

    // Initialize the instances.
    for (uint32_t i = 0; i < ARRAY_SIZE(m_instance_acc_control); i++)
    {
        lwm2m_instance_access_control_init(&m_instance_acc_control[i], i);
        m_instance_acc_control[i].proto.callback = access_control_instance_callback;
        m_instance_acc_control[i].object_id = LWM2M_INVALID_INSTANCE;
        m_instance_acc_control[i].control_owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
    }
}

void lwm2m_access_control_acl_init(void)
{
    size_t progress;
    lwm2m_instance_t *p_instance = NULL;

    while (lwm2m_instance_next(&p_instance, &progress))
    {
        /* The Access Control instances binded to the server instances in Verizon are
           initialised during factory-bootstrap or bootstrap itself. */
        if ((p_instance->object_id == LWM2M_OBJ_SERVER &&
            operator_is_vzw(true)))
        {
            continue;
        }

        if (p_instance->object_id == LWM2M_OBJ_SECURITY ||
            p_instance->object_id == LWM2M_OBJ_ACCESS_CONTROL)
        {
            continue;
        }

        if (p_instance->object_id == LWM2M_OBJ_FIRMWARE)
        {
            lwm2m_firmware_init_acl();
            continue;
        }

        lwm2m_access_control_carrier_acl_set(p_instance->object_id, p_instance->instance_id);
    }
}
