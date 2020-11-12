/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdio.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_time.h>
#include <lwm2m_objects.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_apn_conn_prof.h>
#include <coap_message.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_pdn.h>
#include <at_interface.h>
#include <lwm2m_instance_storage.h>
#include <operator_check.h>
#include <lwm2m_access_control.h>

#define LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE  1
#define LWM2M_APN_CONN_PROF_DEFAULT_INSTANCE 2

static lwm2m_object_t        m_object_apn_conn_prof;                             /**< APN Connection Profile base object. */
// TODO: Only two instances are currently supported.
static lwm2m_apn_conn_prof_t m_instance_apn_conn_prof[LWM2M_MAX_APN_COUNT];      /**< APN Connection Profile object instances. */

static char *                m_profile_name_default[] = { "AT&T LWM2M APN", NULL, NULL };
static char *                m_apn_default[] = { "attm2mglobal", NULL, NULL };
static uint16_t              m_default_apn_instance;

// LWM2M core resources.

lwm2m_apn_conn_prof_t * lwm2m_apn_conn_prof_get_instance(uint16_t instance_id)
{
    if (instance_id >= ARRAY_SIZE(m_instance_apn_conn_prof))
    {
        return NULL;
    }

    return &m_instance_apn_conn_prof[instance_id];
}

lwm2m_object_t * lwm2m_apn_conn_prof_get_object(void)
{
    return &m_object_apn_conn_prof;
}

char * lwm2m_apn_conn_prof_apn_get(uint16_t instance_id, uint8_t * p_len)
{
    // TODO: Fetch from instance_id
    *p_len = m_instance_apn_conn_prof[instance_id].apn.len;
    return m_instance_apn_conn_prof[instance_id].apn.p_val;
}

bool lwm2m_apn_conn_prof_enabled_set(uint16_t instance_id, bool enable_status)
{
    if (instance_id >= ARRAY_SIZE(m_instance_apn_conn_prof))
    {
        return false;
    }

    m_instance_apn_conn_prof[instance_id].enable_status = enable_status;

    lwm2m_string_t *apn = &m_instance_apn_conn_prof[instance_id].apn;
    at_write_apn_status(enable_status ? 1 : 0, apn->p_val, apn->len);

    return true;
}

bool lwm2m_apn_conn_prof_is_enabled(uint16_t instance_id)
{
    if (instance_id >= ARRAY_SIZE(m_instance_apn_conn_prof))
    {
        return false;
    }

    return m_instance_apn_conn_prof[instance_id].enable_status;
}

uint16_t lwm2m_apn_conn_prof_default_instance(void)
{
    return m_default_apn_instance;
}

static uint32_t list_integer_copy(lwm2m_list_t * p_list, int from_idx, int to_idx)
{
    int32_t value = lwm2m_list_integer_get(p_list, from_idx);
    return lwm2m_list_integer_set(p_list, to_idx, value);
}

bool lwm2m_apn_conn_prof_activate(uint16_t instance_id,
                                  uint8_t  reject_cause)
{
    lwm2m_instance_t *p_instance;

    if (lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, instance_id) != 0)
    {
        return false;
    }

    lwm2m_apn_conn_prof_t *apn_conn = (lwm2m_apn_conn_prof_t *)p_instance;

    int apn_idx = apn_conn->conn_est_time.len;
    if (apn_idx == apn_conn->conn_est_time.max_len)
    {
        // List is full, move all values one index down.
        for (int i = 1; i < apn_conn->conn_est_time.max_len; i++)
        {
            list_integer_copy(&apn_conn->conn_est_time, i, i - 1);
            list_integer_copy(&apn_conn->conn_est_result, i, i - 1);
            list_integer_copy(&apn_conn->conn_est_reject_cause, i, i - 1);
            list_integer_copy(&apn_conn->conn_end_time, i, i - 1);
        }
        apn_idx--;
    }

    int32_t utc_time = lwm2m_utc_time();
    lwm2m_list_integer_set(&apn_conn->conn_est_time, apn_idx, utc_time);
    lwm2m_list_integer_set(&apn_conn->conn_est_result, apn_idx, (reject_cause == 0) ? 0 : 1);
    lwm2m_list_integer_set(&apn_conn->conn_est_reject_cause, apn_idx, reject_cause);
    lwm2m_list_integer_set(&apn_conn->conn_end_time, apn_idx, (reject_cause == 0) ? 0 : utc_time);

    lwm2m_storage_apn_conn_prof_store();

    return true;
}

bool lwm2m_apn_conn_prof_deactivate(uint16_t instance_id)
{
    lwm2m_instance_t *p_instance;

    if (lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, instance_id) != 0)
    {
        return false;
    }

    lwm2m_apn_conn_prof_t *apn_conn = (lwm2m_apn_conn_prof_t *)p_instance;

    int apn_idx = 0;
    while ((apn_idx < apn_conn->conn_est_time.max_len) &&
           (lwm2m_list_integer_get(&apn_conn->conn_est_time, apn_idx) != 0))
    {
        apn_idx++;
    }

    if (apn_idx == 0)
    {
        return false;
    }

    lwm2m_list_integer_set(&apn_conn->conn_end_time, apn_idx - 1, lwm2m_utc_time());

    lwm2m_storage_apn_conn_prof_store();

    return true;
}

uint32_t lwm2m_apn_conn_prof_custom_apn_set(const char * p_apn)
{
    lwm2m_instance_t *p_instance;
    uint32_t err_code;
    uint8_t apn_status[128];
    uint8_t apn_quoted[64];

    if (!p_apn || strlen(p_apn) == 0)
    {
        return EINVAL;
    }

    if (!operator_is_att(true))
    {
        return EPERM;
    }

    err_code = lwm2m_bytebuffer_to_string(p_apn, strlen(p_apn), &m_instance_apn_conn_prof[LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE].apn);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_bytebuffer_to_string(p_apn, strlen(p_apn), &m_instance_apn_conn_prof[LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE].profile_name);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE);
    if (err_code == ENOENT)
    {
        err_code = lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_apn_conn_prof[LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE]);
        lwm2m_access_control_carrier_acl_set(LWM2M_OBJ_APN_CONNECTION_PROFILE, m_instance_apn_conn_prof[LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE].proto.instance_id);
    }

    if (at_read_apn_status(apn_status, sizeof(apn_status)) != 0)
    {
        LWM2M_ERR("Error reading APN status");
    }

    snprintf(apn_quoted, sizeof(apn_quoted), "\"%s\"", p_apn);

    m_instance_apn_conn_prof[LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE].enable_status = (strstr(apn_status, apn_quoted) == NULL) ? true : false;

    lwm2m_storage_apn_conn_prof_store();
    lwm2m_storage_access_control_store();

    return err_code;
}

/**@brief Callback function for APN connection profile instances. */
uint32_t apn_conn_prof_instance_callback(lwm2m_instance_t * p_instance,
                                         uint16_t           resource_id,
                                         uint8_t            op_code,
                                         coap_message_t *   p_request)
{
    LWM2M_TRC("apn_conn_prof_instance_callback");

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
    lwm2m_apn_conn_prof_t *p_apn_conn_prof = lwm2m_apn_conn_prof_get_instance(instance_id);

    if (!p_apn_conn_prof)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[200];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {

        err_code = lwm2m_tlv_apn_connection_profile_encode(buffer,
                                                           &buffer_size,
                                                           resource_id,
                                                           &m_instance_apn_conn_prof[instance_id]);
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

        bool previous_status = m_instance_apn_conn_prof[instance_id].enable_status;

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_apn_connection_profile_decode(&m_instance_apn_conn_prof[instance_id],
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
            if (previous_status != m_instance_apn_conn_prof[instance_id].enable_status)
            {
                lwm2m_apn_conn_prof_enabled_set(instance_id, !previous_status);
            }

            if (lwm2m_storage_apn_conn_prof_store() == 0)
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

/**@brief Callback function for LWM2M APN connection profile objects. */
uint32_t lwm2m_apn_conn_prof_object_callback(lwm2m_object_t * p_object,
                                             uint16_t         instance_id,
                                             uint8_t          op_code,
                                             coap_message_t * p_request)
{
    LWM2M_TRC("apn_connection_profile_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint32_t           buffer_max_size = 100 * ARRAY_SIZE(m_instance_apn_conn_prof);
        uint32_t           buffer_len      = buffer_max_size;
        uint8_t            buffer[buffer_max_size];
        uint32_t           index           = 0;

        uint32_t instance_buffer_len = 100;
        uint8_t  instance_buffer[instance_buffer_len];

        for (int i = 0; i < ARRAY_SIZE(m_instance_apn_conn_prof); i++)
        {
            uint16_t access = 0;
            lwm2m_instance_t * p_instance;

            if (lwm2m_lookup_instance(&p_instance, LWM2M_OBJ_APN_CONNECTION_PROFILE, i) != 0)
            {
                continue;
            }

            err_code = lwm2m_access_control_access_remote_get(&access,
                                                              p_instance->object_id,
                                                              p_instance->instance_id,
                                                              p_request->remote);

            if (err_code != 0 || (access & op_code) == 0)
            {
                continue;
            }

            instance_buffer_len = 100;
            err_code = lwm2m_tlv_apn_connection_profile_encode(instance_buffer,
                                                               &instance_buffer_len,
                                                               LWM2M_NAMED_OBJECT,
                                                               lwm2m_apn_conn_prof_get_instance(i));

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
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

void lwm2m_apn_conn_prof_apn_status_update(void)
{
    uint8_t apn_status[128];
    uint8_t apn_quoted[64];
    lwm2m_apn_conn_prof_t *p_instance;
    int offset;

    if (at_read_apn_status(apn_status, sizeof(apn_status)) != 0)
    {
        LWM2M_ERR("Error reading APN status");
        return;
    }

    for (int i = 0; i < ARRAY_SIZE(m_instance_apn_conn_prof); i++)
    {
        p_instance = lwm2m_apn_conn_prof_get_instance(i);

        if (!p_instance || !p_instance->apn.p_val)
        {
            continue;
        }

        offset = 0;

        apn_quoted[offset++] = '"';
        memcpy(&apn_quoted[offset], p_instance->apn.p_val, p_instance->apn.len);
        offset += p_instance->apn.len;
        apn_quoted[offset++] = '"';
        apn_quoted[offset] = '\0';

        p_instance->enable_status = (strstr(apn_status, apn_quoted) == NULL) ? true : false;
    }
}

void lwm2m_apn_conn_prof_init(void)
{
    m_object_apn_conn_prof.object_id = LWM2M_OBJ_APN_CONNECTION_PROFILE;
    m_object_apn_conn_prof.callback = lwm2m_apn_conn_prof_object_callback;

    for (uint16_t i = 0; i < ARRAY_SIZE(m_instance_apn_conn_prof); i++)
    {
        lwm2m_instance_apn_connection_profile_init(&m_instance_apn_conn_prof[i], i);

        m_instance_apn_conn_prof[i].proto.callback = apn_conn_prof_instance_callback;

        char * p_apn = m_apn_default[i];
        if (i == LWM2M_APN_CONN_PROF_DEFAULT_INSTANCE)
        {
            p_apn = lwm2m_pdn_default_apn();
            m_default_apn_instance = i;
        }

        m_instance_apn_conn_prof[i].authentication_type = 0;
        m_instance_apn_conn_prof[i].enable_status = false;

        if (i != LWM2M_APN_CONN_PROF_CUSTOM_INSTANCE)
        {
            if (m_profile_name_default[i])
            {
                lwm2m_bytebuffer_to_string(m_profile_name_default[i], strlen(m_profile_name_default[i]), &m_instance_apn_conn_prof[i].profile_name);
            }
            else
            {
                lwm2m_bytebuffer_to_string(p_apn, strlen(p_apn), &m_instance_apn_conn_prof[i].profile_name);
            }
            lwm2m_bytebuffer_to_string(p_apn, strlen(p_apn), &m_instance_apn_conn_prof[i].apn);

            lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_apn_conn_prof[i]);
        }
    }
}
