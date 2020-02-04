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

static uint32_t m_observer_sequence_num = 0;

uint32_t lwm2m_respond_with_code(coap_msg_code_t code, coap_message_t * p_request)
{
    NULL_PARAM_CHECK(p_request);

    // Application helper function, no need for mutex.
    coap_message_conf_t response_config;
    memset(&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code        = code;
    response_config.id          = p_request->header.id;
    response_config.transport = p_request->transport;

    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t         err_code = coap_message_new(&p_response, &response_config);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = coap_message_remote_addr_set(p_response, p_request->remote);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

    return err_code;
}

uint32_t lwm2m_observe_register(uint8_t             * p_payload,
                                uint16_t              payload_len,
                                uint16_t              max_age,
                                coap_message_t      * p_request,
                                coap_content_type_t   content_type,
                                uint16_t              resource,
                                lwm2m_instance_t    * p_instance)
{
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(p_payload);
    NULL_PARAM_CHECK(p_instance);

    uint32_t        err_code;
    uint32_t        handle;
    coap_observer_t observer;
    uint16_t      * p_resource_ids = (uint16_t *)((uint8_t *)p_instance + p_instance->resource_ids_offset);

    observer.token_len            =  p_request->header.token_len;

    /* !WARNING! This is not a coap_resource_t object !WARNING! */
    observer.resource_of_interest = (coap_resource_t *)&p_resource_ids[resource];

    observer.remote               = p_request->remote;
    observer.transport            = p_request->transport;
    observer.ct                   = content_type;
    observer.p_userdata           = (void*)p_instance;

    memcpy(observer.token, p_request->token, observer.token_len);

    err_code = coap_observe_server_register(&handle, &observer);

    if (err_code != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
    }
    else
    {
        lwm2m_observer_storage_store(&observer);

        // Application helper function, no need for mutex.
        coap_message_conf_t response_config;
        memset(&response_config, 0, sizeof(coap_message_conf_t));

        if (p_request->header.type == COAP_TYPE_NON)
        {
            response_config.type = COAP_TYPE_NON;
        }
        else if (p_request->header.type == COAP_TYPE_CON)
        {
            response_config.type = COAP_TYPE_ACK;
        }

        // PIGGY BACKED RESPONSE
        response_config.code        = COAP_CODE_205_CONTENT;
        response_config.id          = p_request->header.id;
        response_config.transport   = p_request->transport;

        // Copy token.
        memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
        // Copy token length.
        response_config.token_len = p_request->header.token_len;

        coap_message_t * p_response;
        err_code = coap_message_new(&p_response, &response_config);
        if (err_code != 0)
        {
            return err_code;
        }

        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_OBSERVE, m_observer_sequence_num++);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        // Set content format.
        err_code = coap_message_opt_uint_add(p_response,
                                            COAP_OPT_CONTENT_FORMAT,
                                            content_type);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, max_age);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        err_code = coap_message_payload_set(p_response, p_payload, payload_len);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        err_code = coap_message_remote_addr_set(p_response, p_request->remote);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        uint32_t msg_handle;
        err_code = coap_message_send(&msg_handle, p_response);
        if (err_code != 0)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }

        err_code = coap_message_delete(p_response);
    }

    return err_code;
}

uint32_t lwm2m_observe_unregister(struct nrf_sockaddr  * p_remote,
                                  void                 * p_resource)
{
    uint32_t handle;
    coap_observer_t * p_observer;

    uint32_t err_code = coap_observe_server_search(&handle,
                                                   p_remote,
                                                   p_resource);
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

bool lwm2m_is_observed(uint16_t short_server_id, const lwm2m_instance_t * p_instance, uint16_t resource_id)
{
    coap_resource_t *p_resource;
    struct nrf_sockaddr *p_remote;
    uint32_t err_code;
    uint32_t handle;
    uint16_t *p_resource_ids;

    if (!p_instance)
    {
        return false;
    }

    err_code = lwm2m_short_server_id_remote_find(&p_remote, short_server_id);
    if (err_code != 0)
    {
        return false;
    }

    p_resource_ids = (uint16_t *)((uint8_t *)p_instance + p_instance->resource_ids_offset);
    p_resource = (coap_resource_t *)&p_resource_ids[resource_id];

    err_code = coap_observe_server_search(&handle, p_remote, p_resource);
    if (err_code == 0)
    {
        return true;
    }

    return false;
}

static void observer_con_message_callback(uint32_t status, void * arg, coap_message_t * p_response)
{
    uint32_t err_code;
    switch (status)
    {
        case ECONNRESET:
            {
            }
        // Fall through.
        case ETIMEDOUT:
            {
                coap_observer_t * p_observer = (coap_observer_t *)arg;
                err_code = lwm2m_observe_unregister((struct nrf_sockaddr *)p_observer->remote,
                                                    (lwm2m_instance_t *)p_observer->resource_of_interest);

                (void)err_code;
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
    uint32_t err_code = 0;
    // Application helper function, no need for mutex.
    coap_message_conf_t response_config;
    memset(&response_config, 0, sizeof(coap_message_conf_t));

    response_config.type = type; /* TODO: add as parameter */
    response_config.code = COAP_CODE_205_CONTENT;
    response_config.response_callback = observer_con_message_callback;

    // Copy token.
    memcpy(&response_config.token[0], &p_observer->token[0], p_observer->token_len);
    // Copy token length.
    response_config.token_len = p_observer->token_len;

    // Set transport (port) to use.
    response_config.transport = p_observer->transport;

    coap_message_t * p_response;
    err_code = coap_message_new(&p_response, &response_config);
    if (err_code != 0)
    {
        return err_code;
    }

    p_observer->last_mid = response_config.id;

    // Set custom misc. argument.
    p_response->arg = p_observer;

    err_code = coap_message_opt_uint_add(p_response, COAP_OPT_OBSERVE, m_observer_sequence_num++);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    // Set content format.
    err_code = coap_message_opt_uint_add(p_response,
                                        COAP_OPT_CONTENT_FORMAT,
                                        p_observer->ct);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, (((lwm2m_instance_t *)(p_observer->resource_of_interest))->expire_time));
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_payload_set(p_response, p_payload, payload_len);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_remote_addr_set(p_response, p_observer->remote);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

    return err_code;
}

uint32_t lwm2m_respond_with_payload(uint8_t             * p_payload,
                                    uint16_t              payload_len,
                                    coap_content_type_t   content_type,
                                    coap_message_t      * p_request)
{
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(p_payload);

    // Application helper function, no need for mutex.
    coap_message_conf_t response_config;
    memset(&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code        = COAP_CODE_205_CONTENT;
    response_config.id          = p_request->header.id;
    response_config.transport   = p_request->transport;

    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t         err_code = coap_message_new(&p_response, &response_config);
    if (err_code != 0)
    {
        return err_code;
    }

    // Set content format.
    err_code = coap_message_opt_uint_add(p_response,
                                         COAP_OPT_CONTENT_FORMAT,
                                         content_type);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_payload_set(p_response, p_payload, payload_len);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_remote_addr_set(p_response, p_request->remote);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

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

    if (resource_id == LWM2M_NAMED_OBJECT)
    {
        // Object Instance
        uint32_t err_code = lwm2m_coap_handler_gen_instance_link(p_instance, buffer, &buffer_len);

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
    }

    return lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);
}