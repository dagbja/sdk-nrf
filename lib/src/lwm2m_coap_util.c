/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_coap_util

#include <string.h>

#include <lwm2m.h>
#include <coap_api.h>


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
    response_config.p_transport = p_request->p_transport;

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

    err_code = coap_message_remote_addr_set(p_response, p_request->p_remote);
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
    response_config.p_transport = p_request->p_transport;

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

    err_code = coap_message_remote_addr_set(p_response, p_request->p_remote);
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
