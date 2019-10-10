/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <coap_api.h>

#define LWM2M_BOOTSTRAP_URI_PATH "bs"
#define TOKEN_START 0x012A

static uint16_t m_token = TOKEN_START;


static uint32_t internal_message_new(coap_message_t         ** pp_msg,
                                     coap_msg_code_t           code,
                                     coap_response_callback_t  callback,
                                     coap_transport_handle_t   transport)
{
    uint32_t            err_code;
    coap_message_conf_t conf;

    memset(&conf, 0, sizeof(coap_message_conf_t));

    conf.type              = COAP_TYPE_CON;
    conf.code              = code;
    conf.response_callback = callback;
    conf.transport         = transport;

    conf.token_len = uint16_encode(m_token, conf.token);

    m_token++;

    err_code = coap_message_new(pp_msg, &conf);

    return err_code;
}


/**@brief Function to be used as callback function upon a bootstrap request. */
static void lwm2m_bootstrap_cb(uint32_t status, void * p_arg, coap_message_t * p_message)
{
    struct sockaddr *p_remote = NULL;
    uint8_t coap_code = 0;

    if (p_message)
    {
        p_remote  = p_message->remote;
        coap_code = p_message->header.code;
    }

    LWM2M_TRC("status: %u, CoAP code: %u", status, coap_code);

    lwm2m_notification(LWM2M_NOTIFCATION_TYPE_BOOTSTRAP,
                       p_remote,
                       coap_code,
                       0);
}


uint32_t internal_lwm2m_bootstrap_init(void)
{
    m_token = TOKEN_START;

    return 0;
}


uint32_t lwm2m_bootstrap(struct sockaddr         * p_remote,
                         lwm2m_client_identity_t * p_id,
                         coap_transport_handle_t   transport)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_id);

    LWM2M_MUTEX_LOCK();

    uint32_t         err_code;
    coap_message_t * p_msg;

    lwm2m_string_t endpoint;

    endpoint.p_val = LWM2M_BOOTSTRAP_URI_PATH;
    endpoint.len   = 2;

    err_code = internal_message_new(&p_msg, COAP_CODE_POST, lwm2m_bootstrap_cb, transport);
    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }

    if (err_code == 0)
    {
        err_code = coap_message_remote_addr_set(p_msg, p_remote);
    }

    if (err_code == 0)
    {
        err_code = coap_message_opt_str_add(p_msg,
                                            COAP_OPT_URI_PATH,
                                            (uint8_t *)endpoint.p_val,
                                            endpoint.len); // end_point length is always 2
    }

    if (err_code == 0)
    {
        char buffer[128];
        buffer[0] = 'e';
        buffer[1] = 'p';
        buffer[2] = '=';
        memcpy(buffer + 3, &p_id->value, p_id->len);

        err_code = coap_message_opt_str_add(p_msg,
                                            COAP_OPT_URI_QUERY,
                                            (uint8_t *)buffer,
                                            p_id->len + 3);
    }

    if (err_code == 0)
    {
        uint32_t msg_handle;
        err_code = coap_message_send(&msg_handle, p_msg);
    }

    if (err_code == 0)
    {
        err_code = coap_message_delete(p_msg);
    }
    else
    {
        // If we have hit an error try to clean up.
        // Return the original error code.
        (void)coap_message_delete(p_msg);
    }

    LWM2M_MUTEX_UNLOCK();

    LWM2M_EXIT();

    return err_code;
}
