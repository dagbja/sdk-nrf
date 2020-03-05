/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <coap_api.h>
#include <lwm2m_remote.h>
#include <lwm2m_acl.h>
#include <lwm2m_observer.h>

static void observer_con_message_callback(uint32_t status, void * arg, coap_message_t * p_response);

static uint32_t m_observer_sequence_num = 0;

uint32_t lwm2m_coap_message_config_set(void * p_data, bool is_response, coap_message_conf_t * p_config)
{
    NULL_PARAM_CHECK(p_data);
    NULL_PARAM_CHECK(p_config);

    memset(p_config, 0, sizeof(coap_message_conf_t));

    if (is_response)
    {
        coap_message_t *p_request = (coap_message_t *)p_data;

        if (p_request->header.type == COAP_TYPE_NON)
        {
            p_config->type = COAP_TYPE_NON;
        }
        else if (p_request->header.type == COAP_TYPE_CON)
        {
            p_config->type = COAP_TYPE_ACK;
        }

        p_config->id = p_request->header.id;
        p_config->transport = p_request->transport;

        memcpy(p_config->token, p_request->token, p_request->header.token_len);
        p_config->token_len = p_request->header.token_len;
    }
    else
    {
        coap_observer_t *p_observer = (coap_observer_t *)p_data;

        p_config->response_callback = observer_con_message_callback;
        p_config->transport = p_observer->transport;

        memcpy(p_config->token, p_observer->token, p_observer->token_len);
        p_config->token_len = p_observer->token_len;
    }

    return 0;
}

/* By design, the options need to be added in ascending order, hence the
   caller of this function needs to ensure the order is respected. */
static uint32_t lwm2m_coap_options_uint_add(coap_message_t * p_message,
                                            uint16_t       * p_opt_num,
                                            uint32_t       * p_opt_val,
                                            uint16_t         opt_count)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_opt_num);
    NULL_PARAM_CHECK(p_opt_val);

    uint32_t err_code = 0;

    for (int i = 0; i < opt_count; i++)
    {
        err_code = coap_message_opt_uint_add(p_message, p_opt_num[i], p_opt_val[i]);
        if (err_code != 0)
        {
            break;
        }
    }

    return err_code;
}

uint32_t lwm2m_coap_message_send_to_remote(coap_message_t      * p_message,
                                           struct nrf_sockaddr * p_remote,
                                           uint8_t             * p_payload,
                                           uint16_t              payload_len)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_remote);

    uint32_t err_code = 0;
    uint32_t msg_handle;

    if (p_payload)
    {
        err_code = coap_message_payload_set(p_message, p_payload, payload_len);
        if (err_code != 0)
        {
            LWM2M_WRN("Failed to set the payload of the message, err %d", err_code);
            return err_code;
        }
    }

    err_code = coap_message_remote_addr_set(p_message, p_remote);
    if (err_code != 0)
    {
        LWM2M_WRN("Failed to set the destination of the message, err %d", err_code);
        return err_code;
    }

    err_code = coap_message_send(&msg_handle, p_message);
    if (err_code != 0)
    {
        LWM2M_WRN("Failed to send the message, err %d", err_code);
        return err_code;
    }

    err_code = coap_message_delete(p_message);
    if (err_code != 0)
    {
        LWM2M_WRN("Failed to delete the message, err %d", err_code);
    }

    return 0;
}

uint32_t lwm2m_respond_with_code(coap_msg_code_t code, coap_message_t * p_request)
{
    NULL_PARAM_CHECK(p_request);

    // Application helper function, no need for mutex.
    coap_message_conf_t config;
    uint32_t err_code;
    coap_message_t *p_response;

    err_code = lwm2m_coap_message_config_set(p_request, true, &config);

    // Set the response code.
    config.code = code;

    err_code = coap_message_new(&p_response, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_coap_message_send_to_remote(p_response, p_request->remote, NULL, 0);

    return err_code;
}

uint32_t lwm2m_observe_register(const uint16_t   * p_path,
                                uint8_t            path_len,
                                coap_message_t   * p_request,
                                coap_message_t  ** pp_response)
{
    NULL_PARAM_CHECK(p_path);
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(pp_response);

    const void * p_observable;
    uint32_t err_code = 0;
    uint32_t handle;
    coap_observer_t observer;
    coap_message_conf_t config;
    uint16_t obs_opt_num[] = { COAP_OPT_OBSERVE, COAP_OPT_CONTENT_FORMAT, COAP_OPT_MAX_AGE };
    uint32_t obs_opt_val[] = { m_observer_sequence_num++, COAP_CT_APP_LWM2M_TLV, 60 };

    p_observable = lwm2m_observable_reference_get(p_path, path_len);
    if (!p_observable)
    {
        return ENOENT;
    }

    /* Create the observer */
    observer.remote = p_request->remote;
    observer.token_len = p_request->header.token_len;
    memcpy(observer.token, p_request->token, observer.token_len);
    observer.ct = COAP_CT_APP_LWM2M_TLV;
    observer.resource_of_interest = p_observable;
    observer.transport = p_request->transport;

    /* Register the observer */
    err_code = coap_observe_server_register(&handle, &observer);
    if (err_code != 0)
    {
        return err_code;
    }

    lwm2m_observer_storage_store(&observer, p_path, path_len);

    err_code = lwm2m_coap_message_config_set(p_request, true, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    config.code = COAP_CODE_205_CONTENT;

    err_code = coap_message_new(pp_response, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_coap_options_uint_add(*pp_response, obs_opt_num, obs_opt_val, ARRAY_SIZE(obs_opt_num));
    if (err_code != 0)
    {
        coap_message_delete(*pp_response);
        *pp_response = NULL;
    }

    return err_code;
}

uint32_t lwm2m_observe_unregister(struct nrf_sockaddr * p_remote, const void * p_observable)
{
    NULL_PARAM_CHECK(p_observable);
    NULL_PARAM_CHECK(p_remote);

    uint32_t handle, err_code;
    coap_observer_t * p_observer;

    err_code = coap_observe_server_search(&handle,
                                          p_remote,
                                          p_observable);
    if (err_code == 0)
    {
        err_code = coap_observe_server_get(handle, &p_observer);
    }

    if (err_code == 0)
    {
        err_code = lwm2m_observer_storage_delete(p_observer);
    }

    if (err_code == 0)
    {
        err_code = coap_observe_server_unregister(handle);
    }

    if (err_code != 0)
    {
        LWM2M_INF("Observer unregister failed: %s (%ld), %s (%d)",
                  lwm2m_os_log_strdup(strerror(err_code)), err_code,
                  lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());
    }

    return err_code;
}

bool lwm2m_is_observed(uint16_t short_server_id, const void * p_observable)
{
    struct nrf_sockaddr *p_remote;
    uint32_t err_code;
    uint32_t handle;

    if (!p_observable)
    {
        return false;
    }

    err_code = lwm2m_short_server_id_remote_find(&p_remote, short_server_id);
    if (err_code != 0)
    {
        return false;
    }

    err_code = coap_observe_server_search(&handle, p_remote, p_observable);
    if (err_code == 0)
    {
        return true;
    }

    return false;
}

static void observer_con_message_callback(uint32_t status, void * arg, coap_message_t * p_response)
{
    switch (status)
    {
        case ECONNRESET:
            {
            }
        // Fall through.
        case ETIMEDOUT:
            {
                coap_observer_t * p_observer = (coap_observer_t *)arg;
                lwm2m_observe_unregister(p_observer->remote,
                                         p_observer->resource_of_interest);
            }
            break;
        default:
            {
                // The CON message went fine.
            }
            break;
    }
}

uint32_t lwm2m_notify(uint8_t         * p_payload,
                      uint16_t          payload_len,
                      coap_observer_t * p_observer,
                      coap_msg_type_t   type)
{
    NULL_PARAM_CHECK(p_payload);
    NULL_PARAM_CHECK(p_observer);

    uint32_t err_code = 0;
    coap_message_conf_t config;
    coap_message_t *p_message;
    uint16_t notif_opt_num[] = { COAP_OPT_OBSERVE, COAP_OPT_CONTENT_FORMAT, COAP_OPT_MAX_AGE };
    uint32_t notif_opt_val[] = { m_observer_sequence_num++, COAP_CT_APP_LWM2M_TLV, 60 };

    err_code = lwm2m_coap_message_config_set(p_observer, false, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    config.code = COAP_CODE_205_CONTENT;
    config.type = type;

    err_code = coap_message_new(&p_message, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    p_observer->last_mid = config.id;
    // Set custom misc. argument.
    p_message->arg = p_observer;

    err_code = lwm2m_coap_options_uint_add(p_message, notif_opt_num, notif_opt_val, ARRAY_SIZE(notif_opt_num));
    if (err_code != 0)
    {
        coap_message_delete(p_message);
        return err_code;
    }

    err_code = lwm2m_coap_message_send_to_remote(p_message, p_observer->remote, p_payload, payload_len);

    return err_code;
}

uint32_t lwm2m_respond_with_payload(uint8_t             * p_payload,
                                    uint16_t              payload_len,
                                    coap_content_type_t   content_type,
                                    coap_message_t      * p_request)
{
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(p_payload);

    coap_message_conf_t config;
    coap_message_t *p_response;
    uint16_t res_opt_num[] = { COAP_OPT_CONTENT_FORMAT };
    uint32_t res_opt_val[] = { content_type };
    uint32_t err_code;

    err_code = lwm2m_coap_message_config_set(p_request, true, &config);
    if (err_code != 0)
    {
       return err_code;
    }

    config.code = COAP_CODE_205_CONTENT;

    err_code = coap_message_new(&p_response, &config);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_coap_options_uint_add(p_response, res_opt_num, res_opt_val, ARRAY_SIZE(res_opt_num));
    if (err_code != 0)
    {
        coap_message_delete(p_response);
        return err_code;
    }

    err_code = lwm2m_coap_message_send_to_remote(p_response, p_request->remote, p_payload, payload_len);

    return err_code;
}

uint32_t lwm2m_respond_with_bs_discover_link(uint16_t object_id, coap_message_t * p_request)
{
    uint32_t  link_format_string_len = 0;
    uint8_t * p_link_format_string   = NULL;

    // Dry run the link format generation, to check how much memory that is needed.
    uint32_t err_code = lwm2m_coap_handler_gen_link_format(object_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID, NULL, (uint16_t *)&link_format_string_len);

    if (err_code == 0) {
        // Allocate the needed amount of memory.
        p_link_format_string = lwm2m_malloc(link_format_string_len);

        if (p_link_format_string == NULL) {
            err_code = ENOMEM;
        }
    }

    if (err_code == 0) {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(object_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID, p_link_format_string, (uint16_t *)&link_format_string_len);
    }

    if (err_code == 0) {
        err_code = lwm2m_respond_with_payload(p_link_format_string, link_format_string_len, COAP_CT_APP_LINK_FORMAT, p_request);
    }

    if (err_code != 0) {
        (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
    }

    if (p_link_format_string) {
        lwm2m_free(p_link_format_string);
    }

    return err_code;
}

uint32_t lwm2m_respond_with_object_link(uint16_t object_id, coap_message_t * p_request)
{
    uint8_t  buffer[512];
    uint32_t buffer_len = sizeof(buffer);

    uint16_t short_server_id = 0;
    uint32_t err_code;

    err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);
    if (err_code != 0)
    {
        // LWM2M remote not found. Setting it to be default short server id.
        short_server_id = LWM2M_ACL_DEFAULT_SHORT_SERVER_ID;
    }

    // Object
    err_code = lwm2m_coap_handler_gen_object_link(object_id, short_server_id, buffer, &buffer_len);

    if (err_code != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
        return err_code;
    }

    return lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);
}

uint32_t lwm2m_respond_with_instance_link(lwm2m_instance_t * p_instance,
                                          uint16_t           resource_id,
                                          coap_message_t   * p_request)
{
    uint8_t  buffer[512];
    uint32_t buffer_len = sizeof(buffer);
    uint32_t added_len, err_code;
    uint16_t short_server_id = 0;
    uint16_t path[] = { p_instance->object_id, p_instance->instance_id, resource_id };

    err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);
    if (err_code != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
        return err_code;
    }

    if (resource_id == LWM2M_NAMED_OBJECT)
    {
        // Object Instance
        err_code = lwm2m_coap_handler_gen_instance_link(p_instance, short_server_id, buffer, &buffer_len);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
            return err_code;
        }
    }
    else
    {
        // Resource
        buffer_len = snprintf(buffer, buffer_len, "</%u/%u/%u>",
                              p_instance->object_id,
                              p_instance->instance_id,
                              resource_id);

        added_len = sizeof(buffer) - buffer_len;
        err_code = lwm2m_coap_handler_gen_attr_link(path, 3, short_server_id, &buffer[buffer_len], &added_len);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
            return err_code;
        }
    }

    return lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);
}
