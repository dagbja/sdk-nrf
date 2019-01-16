/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_coap_util

#include <string.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <net/coap_api.h>

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

    u32_t msg_handle;
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
                                lwm2m_instance_t    * p_instance_proto)
{
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(p_payload);

    uint32_t err_code;

    // Register observer, and if successful, add the Observe option in the reply.
    u32_t handle;
    coap_observer_t observer;

    // Set the token length.
    observer.token_len              = p_request->header.token_len;
    // Set the resource of interest.
    observer.resource_of_interest = (coap_resource_t *)p_instance_proto;
    // Set the remote.
    observer.remote = p_request->remote;
    // Set the transport where to send notifications.
    observer.transport = p_request->transport;
    // Set the token.
    memcpy(observer.token, p_request->token, observer.token_len);

    // Set the content format to be used for subsequent notifications.
    observer.ct = content_type;

    err_code = coap_observe_server_register(&handle, &observer);

    if (err_code != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
    }
    else
    {
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
        response_config.transport = p_request->transport;

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

        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, p_instance_proto->expire_time);
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

        u32_t msg_handle;
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

static void observer_con_message_callback(u32_t status, void * arg, coap_message_t * p_response)
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

                // Remove observer from its list.
                u32_t handle;
                err_code = coap_observe_server_search(&handle,
                                                      (struct sockaddr *)p_observer->remote,
                                                      (coap_resource_t *)p_observer->resource_of_interest);
                (void)err_code;

                err_code = coap_observe_server_unregister(handle);
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

    u32_t msg_handle;
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

    u32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != 0)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

    return err_code;
}
