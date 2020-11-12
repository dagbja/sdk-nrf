/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_register.h>
#include <lwm2m_remote.h>
#include <coap_api.h>

#define LWM2M_REGISTER_URI_PATH "rd"
#define TOKEN_START 0xAE1C

static uint16_t m_token = TOKEN_START;


static uint32_t internal_message_new(coap_message_t **         pp_msg,
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
    conf.token_len         = uint16_encode(m_token, conf.token);
    m_token++;

    err_code = coap_message_new(pp_msg, &conf);

    return err_code;
}


static uint32_t internal_server_config_set(coap_message_t * msg, lwm2m_server_config_t * p_config)
{
    char     buffer[32];
    uint32_t err_code = 0;

    if (p_config->lifetime > 0)
    {
        int retval = snprintf(buffer, sizeof(buffer), "lt=%u", p_config->lifetime);
        if (retval < 0)
        {
            err_code = EINVAL;
        }
        else
        {
            err_code = coap_message_opt_str_add(msg,
                                                COAP_OPT_URI_QUERY,
                                                (uint8_t *)buffer,
                                                strlen(buffer));
        }
    }

    if (err_code == 0)
    {
        if ((p_config->lwm2m_version_major > 0) || (p_config->lwm2m_version_minor > 0))
        {
            int retval = snprintf(buffer,
                                  sizeof(buffer),
                                  "lwm2m=%d.%d",
                                  p_config->lwm2m_version_major,
                                  p_config->lwm2m_version_minor);
            if (retval < 0)
            {
                err_code = EINVAL;
            }
            else
            {
                err_code = coap_message_opt_str_add(msg,
                                                    COAP_OPT_URI_QUERY,
                                                    (uint8_t *)buffer,
                                                    strlen(buffer));
            }
        }
    }

    if (err_code == 0)
    {
        if (p_config->msisdn.p_val != NULL && p_config->msisdn.len != 0)
        {
            if (p_config->msisdn.len < sizeof(buffer) - 4)
            {
                buffer[0] = 's';
                buffer[1] = 'm';
                buffer[2] = 's';
                buffer[3] = '=';
                memcpy(buffer + 4, p_config->msisdn.p_val, p_config->msisdn.len);

                err_code = coap_message_opt_str_add(msg,
                                                    COAP_OPT_URI_QUERY,
                                                    (uint8_t *)buffer,
                                                    p_config->msisdn.len + 4);
            }
            else
            {
                err_code = ENOMEM;
            }
        }
    }

    if (err_code == 0)
    {
        if (p_config->binding.len > 0)
        {
            if (p_config->binding.len < sizeof(buffer) - 2)
            {
                buffer[0] = 'b';
                buffer[1] = '=';
                memcpy(buffer + 2, p_config->binding.p_val, p_config->binding.len);

                err_code = coap_message_opt_str_add(msg,
                                                    COAP_OPT_URI_QUERY,
                                                    (uint8_t *)buffer,
                                                    p_config->binding.len + 2);
            }
            else
            {
                err_code = ENOMEM;
            }
        }
    }

    return err_code;
}

static uint32_t add_vendor_options(coap_message_t          * p_msg,
                                   coap_option_with_type_t * p_options,
                                   uint8_t                   num_options)
{
    uint32_t err_code = 0;
    uint16_t last_opt_num = 0;

    if ((p_msg == NULL) ||
        ((p_options == NULL) && (num_options > 0)))
    {
        return EINVAL;
    }

    for (int i = 0; i < num_options; ++i)
    {
        if (err_code == 0)
        {
            if (p_options[i].coap_opts.number < last_opt_num) {
                LWM2M_ERR("vendor_options out of sequence");
                return EINVAL;
            }
            last_opt_num = p_options[i].coap_opts.number;

            switch (p_options[i].type)
            {
            case COAP_OPT_TYPE_EMPTY:
                err_code = coap_message_opt_empty_add(p_msg,
                    p_options[i].coap_opts.number);
                break;

            case COAP_OPT_TYPE_UINT:
                err_code = coap_message_opt_uint_add(p_msg,
                    p_options[i].coap_opts.number,
                    p_options[i].coap_opts.data[0]);
                break;

            case COAP_OPT_TYPE_STRING:
                err_code = coap_message_opt_str_add(p_msg,
                    p_options[i].coap_opts.number,
                    p_options[i].coap_opts.data,
                    p_options[i].coap_opts.length);
                break;

            case COAP_OPT_TYPE_OPAQUE:
                err_code = coap_message_opt_opaque_add(p_msg,
                    p_options[i].coap_opts.number,
                    p_options[i].coap_opts.data,
                    p_options[i].coap_opts.length);
                break;

            default:
                break;
            }
        }
    }

    return err_code;
}

uint32_t internal_lwm2m_register_init(void)
{
    m_token       = TOKEN_START;
    return 0;
}


static void lwm2m_register_cb(uint32_t status, void * p_arg, coap_message_t * p_message)
{
    struct nrf_sockaddr *p_remote = NULL;
    uint8_t coap_code = 0;
    uint32_t err_code = 0;

    if (p_message)
    {
        p_remote = p_message->remote;
        coap_code = p_message->header.code;
    }

    LWM2M_TRC("status: %u, CoAP code: %u", status, coap_code);

    if (p_message)
    {
        LWM2M_MUTEX_LOCK();

        // Get the short server id.
        uint16_t short_server_id = (uint32_t)p_arg;

        // Save the remote to lwm2m_remote. Pass the error to lwm2m_notification if we failed.
        err_code = lwm2m_remote_register(short_server_id, p_remote);

        if (err_code == 0)
        {
            for (uint16_t i = 0; i < p_message->options_count; ++i)
            {
                coap_option_t option = p_message->options[i];

                if (option.number == COAP_OPT_LOCATION_PATH)
                {
                    err_code = lwm2m_remote_location_save((char *)option.data,
                                                          option.length,
                                                          short_server_id);
                }
            }
        }

        LWM2M_MUTEX_UNLOCK();
    }

    lwm2m_notification(LWM2M_NOTIFCATION_TYPE_REGISTER,
                       p_remote,
                       coap_code,
                       err_code);
}


uint32_t lwm2m_register(struct nrf_sockaddr     * p_remote,
                        lwm2m_client_identity_t * p_id,
                        lwm2m_server_config_t   * p_config,
                        coap_transport_handle_t   transport,
                        uint8_t                 * p_link_format_string,
                        uint16_t                  link_format_len)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_id);
    NULL_PARAM_CHECK(p_config);
    NULL_PARAM_CHECK(p_link_format_string);

    LWM2M_MUTEX_LOCK();

    uint32_t         err_code;
    coap_message_t * p_msg;
    char             buffer[128];
    lwm2m_string_t   uri_path;

    uri_path.p_val = LWM2M_REGISTER_URI_PATH;
    uri_path.len   = 2;

    err_code = internal_message_new(&p_msg, COAP_CODE_POST, lwm2m_register_cb, transport);
    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }

    if (err_code == 0)
    {
        uint32_t ssid = p_config->short_server_id;
        p_msg->arg    = (void *)ssid;
        err_code      = coap_message_remote_addr_set(p_msg, p_remote);
    }

    if (err_code == 0)
    {
        // Set uri-path option
        err_code = coap_message_opt_str_add(p_msg,
                                            COAP_OPT_URI_PATH,
                                            (uint8_t *)uri_path.p_val,
                                            uri_path.len);
    }

    if (err_code == 0)
    {
        // Set content format.
        err_code = coap_message_opt_uint_add(p_msg,
                                             COAP_OPT_CONTENT_FORMAT,
                                             COAP_CT_APP_LINK_FORMAT);
    }

    if (err_code == 0)
    {
        // Set queries.
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
        err_code = internal_server_config_set(p_msg, p_config);
    }

    if (err_code == 0)
    {
        // Add vendor-specific options
        err_code = add_vendor_options(p_msg, p_config->p_options, p_config->num_options);
    }

    if (err_code == 0)
    {
        err_code = coap_message_payload_set(p_msg, p_link_format_string, link_format_len);
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


void lwm2m_update_cb(uint32_t status, void * p_arg, coap_message_t * p_message)
{
    struct nrf_sockaddr *p_remote = NULL;
    uint8_t coap_code = 0;

    if (p_message)
    {
        p_remote = p_message->remote;
        coap_code = p_message->header.code;
    }

    LWM2M_TRC("status: %u, CoAP code: %u", status, coap_code);

    lwm2m_notification(LWM2M_NOTIFCATION_TYPE_UPDATE,
                       p_remote,
                       coap_code,
                       0);
}


uint32_t lwm2m_update(struct nrf_sockaddr     * p_remote,
                      lwm2m_server_config_t   * p_config,
                      coap_transport_handle_t   transport)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_config);

    LWM2M_MUTEX_LOCK();

    uint32_t         err_code;
    coap_message_t * p_msg;
    lwm2m_string_t   uri_path;

    uri_path.p_val = LWM2M_REGISTER_URI_PATH;
    uri_path.len   = 2;

    err_code = internal_message_new(&p_msg, COAP_CODE_POST, lwm2m_update_cb, transport);
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
        // Set uri-path option
        err_code = coap_message_opt_str_add(p_msg,
                                            COAP_OPT_URI_PATH,
                                            (uint8_t *)uri_path.p_val,
                                            uri_path.len);
    }

    if (err_code == 0)
    {
        char   * p_location;
        uint16_t location_len;

        err_code = lwm2m_remote_location_find(&p_location,
                                              &location_len,
                                              p_config->short_server_id);

        if (err_code == 0)
        {
            // Sets URI PATH
            err_code = coap_message_opt_str_add(p_msg,
                                                COAP_OPT_URI_PATH,
                                                (uint8_t *)p_location,
                                                location_len);
        }
    }

    if (err_code == 0)
    {
        // Sets CoAP queries
        err_code = internal_server_config_set(p_msg, p_config);
    }

    if (err_code == 0)
    {
        // Add vendor-specific options
        err_code = add_vendor_options(p_msg, p_config->p_options, p_config->num_options);
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

    LWM2M_EXIT();

    LWM2M_MUTEX_UNLOCK();

    return err_code;
}


void lwm2m_deregister_cb(uint32_t status, void * p_arg, coap_message_t * p_message)
{
    struct nrf_sockaddr *p_remote = NULL;
    uint8_t coap_code = 0;
    uint32_t err_code = 0;

    if (p_message)
    {
        p_remote = p_message->remote;
        coap_code = p_message->header.code;
    }

    LWM2M_TRC("status: %u, CoAP code: %u", status, coap_code);


    lwm2m_notification(LWM2M_NOTIFCATION_TYPE_DEREGISTER,
                       p_remote,
                       coap_code,
                       err_code);

   /* Deregister will happen only if a valid response is received upon sending
    the deregister request. Otherwise, if an empty message (coap_code == 0)
    is received, the SSID cannot be removed from the database yet, as a deregister
    timeout will take place to reestablish the connection with the server. */
    if (p_message && coap_code != 0)
    {
        LWM2M_MUTEX_LOCK();

        // Find short_server_id of the remote.
        uint16_t ssid;
        err_code = lwm2m_remote_short_server_id_find(&ssid, p_remote);

        if (err_code == 0)
        {
            err_code = lwm2m_remote_deregister(ssid);
        }

        LWM2M_MUTEX_UNLOCK();
    }
}


uint32_t lwm2m_deregister(struct nrf_sockaddr * p_remote, coap_transport_handle_t transport)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_remote);

    LWM2M_MUTEX_LOCK();

    uint32_t         err_code;
    coap_message_t * p_msg;

    lwm2m_string_t uri_path;

    uri_path.p_val = LWM2M_REGISTER_URI_PATH;
    uri_path.len   = 2;

    err_code = internal_message_new(&p_msg, COAP_CODE_DELETE, lwm2m_deregister_cb, transport);
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
        // Set uri-path option
        err_code = coap_message_opt_str_add(p_msg,
                                            COAP_OPT_URI_PATH,
                                            (uint8_t *)uri_path.p_val,
                                            uri_path.len);
    }

    if (err_code == 0)
    {
        // Find the short_server_id of this remote.
        // TODO error check.
        uint16_t ssid;
        err_code = lwm2m_remote_short_server_id_find(&ssid, p_remote);

        char   * p_location;
        uint16_t location_len;
        // TODO error check.
        err_code = lwm2m_remote_location_find(&p_location, &location_len, ssid);


        if (err_code == 0)
        {
            err_code = coap_message_opt_str_add(p_msg,
                                                COAP_OPT_URI_PATH,
                                                (uint8_t *)p_location,
                                                location_len);
        }
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
