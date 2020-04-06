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

#define LWM2M_PORTFOLIO_MAX_INSTANCES    3
#define LWM2M_PORTFOLIO_CARRIER_INSTANCE 2

static lwm2m_object_t    m_object_portfolio;                                  /**< Portfolio base object. */
static lwm2m_portfolio_t m_instance_portfolio[LWM2M_PORTFOLIO_MAX_INSTANCES]; /**< Portfolio object instance. */
static char *            m_portfolio_identity_val[][LWM2M_PORTFOLIO_IDENTITY_INSTANCES] =
{
    { HOST_DEVICE_ID_0, HOST_DEVICE_MANUFACTURER_0, HOST_DEVICE_MODEL_0, HOST_DEVICE_SW_VERSION_0 },
    { HOST_DEVICE_ID_1, HOST_DEVICE_MANUFACTURER_1, HOST_DEVICE_MODEL_1, HOST_DEVICE_SW_VERSION_1 }
};
static lwm2m_string_t    m_portfolio_identity[ARRAY_SIZE(m_instance_portfolio)][LWM2M_PORTFOLIO_IDENTITY_INSTANCES];

static void on_object_read(coap_message_t *p_req);

static bool operation_is_allowed(uint16_t inst, uint16_t res, uint16_t op)
{
    lwm2m_portfolio_t *p_instance = lwm2m_portfolio_get_instance(inst);

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

lwm2m_portfolio_t * lwm2m_portfolio_get_instance(uint16_t instance_id)
{
    lwm2m_portfolio_t *p_instance = NULL;

    for (int i = 0; i < ARRAY_SIZE(m_instance_portfolio); i++)
    {
        if (m_instance_portfolio[i].proto.instance_id == instance_id)
        {
            p_instance = &m_instance_portfolio[i];
        }
    }

    return p_instance;
}

lwm2m_object_t * lwm2m_portfolio_get_object(void)
{
    return &m_object_portfolio;
}

static void on_read(const uint16_t path[3], uint8_t path_len,
                    coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[100];
    size_t len;
    lwm2m_portfolio_t *p_instance;

    const uint16_t inst = path[1];
    const uint16_t res = path[2];

    p_instance = lwm2m_portfolio_get_instance(inst);

    len = sizeof(buf);
    err = lwm2m_tlv_portfolio_encode(buf, &len, res, p_instance);
    if (err) {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_write_attribute(const uint16_t path[3], uint8_t path_len,
                               coap_message_t *p_req)
{
    int err;

    err = lwm2m_write_attribute_handler(path, path_len, p_req);
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
    lwm2m_portfolio_t *p_instance;

    const uint16_t inst = path[1];

    p_instance = lwm2m_portfolio_get_instance(inst);

    err = coap_message_ct_mask_get(p_req, &mask);
    if (err) {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        return;
    }

    if (mask & COAP_CT_MASK_APP_LWM2M_TLV) {
        /* Decode TLV payload */
        err = lwm2m_tlv_portfolio_decode(p_instance,
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

    if (inst == 0)
    {
        int ret = at_write_host_device_info(&m_instance_portfolio[inst].identity);
        if (ret != 0)
        {
            LWM2M_WRN("AT+ODIS failed: %d", ret);
        }
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_observe_start(const uint16_t path[3], uint8_t path_len,
                             coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[300];
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

    err = lwm2m_observable_metadata_init(p_req->remote, path, path_len);
    if (err) {
        /* Already logged */
    }
}

static void on_observe_stop(const uint16_t path[3], uint8_t path_len,
                            coap_message_t *p_req)
{
    uint32_t err;

    const void * p_observable = lwm2m_observable_reference_get(path, path_len);

    LWM2M_INF("Observe deregister %s",
              lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));

    err = lwm2m_observe_unregister(p_req->remote, p_observable);

    if (err) {
        /* TODO */
    }

    lwm2m_notif_attr_storage_update(path, path_len, p_req->remote);

    /* Process as a read */
    if (path_len == 1) {
        on_object_read(p_req);
    } else {
        on_read(path, path_len, p_req);
    }
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

static void on_discover(const uint16_t path[3], uint8_t path_len,
                        coap_message_t *p_req)
{
    uint32_t err;
    lwm2m_portfolio_t *p_instance;

    const uint16_t inst = path[1];
    const uint16_t res = path[2];

    p_instance = lwm2m_portfolio_get_instance(inst);

    err = lwm2m_respond_with_instance_link(&p_instance->proto, res, p_req);
    if (err) {
        LWM2M_WRN("Failed to respond to discover on %s, err %d",
            lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)), err);
    }
}

/**@brief Callback function for portfolio instances. */
uint32_t portfolio_instance_callback(lwm2m_instance_t * p_instance,
                                     uint16_t           resource_id,
                                     uint8_t            op_code,
                                     coap_message_t *   p_request)
{
    uint16_t access;
    uint32_t err_code;
    lwm2m_instance_t *p_portfolio;

    const uint8_t path_len = (resource_id == LWM2M_NAMED_OBJECT) ? 2 : 3;
    const uint16_t path[] = {
        p_instance->object_id,
        p_instance->instance_id,
        resource_id
    };

    err_code = lwm2m_access_remote_get(&access, p_instance, p_request->remote);
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
    if (!operation_is_allowed(path[1], path[2], op_code)) {
        LWM2M_WRN("Operation 0x%x on %s, not allowed", op_code,
                  lwm2m_os_log_strdup(lwm2m_path_to_string(path, path_len)));
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        return 0;
    }

    // The last instance might have not been created yet.
    if (lwm2m_lookup_instance(&p_portfolio, LWM2M_OBJ_PORTFOLIO, p_instance->instance_id) != 0) {
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

    const uint16_t path[] = { LWM2M_OBJ_PORTFOLIO };

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

    err = lwm2m_respond_with_object_link(LWM2M_OBJ_PORTFOLIO, p_req);
    if (err) {
        LWM2M_WRN("Failed to discover portfolio object, err %d", err);
    }
}

int lwm2m_portfolio_instance_create(uint16_t instance_id)
{
    int err;
    lwm2m_instance_t *p_instance;
    uint16_t *p_instance_id;

    p_instance_id = &m_instance_portfolio[LWM2M_PORTFOLIO_CARRIER_INSTANCE].proto.instance_id;

    /* Check if the instance has already been added to the handler. */
    err = lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_PORTFOLIO, *p_instance_id);
    if (err == 0)
    {
        LWM2M_WRN("Failed to create a new portfolio object instance, no slots available");
        return -ENOMEM;
    }

    /* Check if the instance identifier is already in use. */
    for (int i = 0; i < ARRAY_SIZE(m_instance_portfolio) - 1; i++)
    {
        if (m_instance_portfolio[i].proto.instance_id == instance_id)
        {
            LWM2M_WRN("Failed to create a new portfolio object instance, identifier already in use");
            return -EINVAL;
        }
    }

    *p_instance_id = instance_id;

    lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_portfolio[LWM2M_PORTFOLIO_CARRIER_INSTANCE]);

    return 0;
}

static void on_object_create(coap_message_t *p_req)
{
    uint32_t err;
    uint16_t instance_id;

    /* Check if the TLV payload specifies the new object instance ID. */
    if (p_req->payload)
    {
        lwm2m_tlv_t tlv;
        uint32_t index = 0;

        err = lwm2m_tlv_decode(&tlv, &index, p_req->payload, p_req->payload_len);

        if (err != 0)
        {
            lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
            return;
        }

        instance_id = tlv.id;
    }
    else
    {
        instance_id = LWM2M_PORTFOLIO_CARRIER_INSTANCE;
    }

    err = lwm2m_portfolio_instance_create(instance_id);

    if (err != 0)
    {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
    }

    lwm2m_respond_with_code(COAP_CODE_201_CREATED, p_req);
}

/**@brief Callback function for LWM2M portfolio objects. */
uint32_t lwm2m_portfolio_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request)
{
    const uint16_t path[] = { LWM2M_OBJ_PORTFOLIO, LWM2M_INVALID_INSTANCE, LWM2M_INVALID_RESOURCE};
    const uint8_t path_len = 1;

    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_object_read(p_request);
        break;
    case LWM2M_OPERATION_CODE_OBSERVE:
        on_observe(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_write_attribute(path, path_len, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_object_discover(p_request);
        break;
    case LWM2M_OPERATION_CODE_CREATE:
        on_object_create(p_request);
        break;
    default:
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        break;
    }

    return 0;
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

        m_instance_portfolio[i].identity.val.p_string = m_portfolio_identity[i];
        m_instance_portfolio[i].identity.len = ARRAY_SIZE(m_portfolio_identity_val[i]);

        // Set bootstrap server as owner.
        (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_portfolio[i],
                                        LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

        /* The last instance is reserved for the carrier and will be added to the
           handler upon a CREATE request. */
        if (i != LWM2M_PORTFOLIO_CARRIER_INSTANCE)
        {
            (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_portfolio[i]);
        }
    }

    uint16_t instance_id = 0;

    if (at_read_host_device_info(&m_instance_portfolio[instance_id].identity) == 0)
    {
        ++instance_id;
    }

    for (int i = instance_id; i < ARRAY_SIZE(m_instance_portfolio) - 1; i++)
    {
        for (int j = 0; j < LWM2M_PORTFOLIO_IDENTITY_INSTANCES; j++)
        {
            (void)lwm2m_bytebuffer_to_string(m_portfolio_identity_val[i][j], strlen(m_portfolio_identity_val[i][j]), &m_portfolio_identity[i][j]);
        }
    }

    // Initialize ACL
    lwm2m_portfolio_init_acl();
}

const void * lwm2m_portfolio_resource_reference_get(uint16_t instance_id, uint16_t resource_id, uint8_t *p_type)
{
    const void *p_observable = NULL;
    uint8_t type;

    switch (resource_id)
    {
    case LWM2M_PORTFOLIO_IDENTITY:
        type = LWM2M_OBSERVABLE_TYPE_LIST;
        p_observable = &m_instance_portfolio[instance_id].identity;
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
