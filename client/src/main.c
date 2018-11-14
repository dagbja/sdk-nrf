/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <net/socket.h>
#include <dk_buttons_and_leds.h>
//#include <lte_lc.h>
#include <nrf.h>

#include <coap_api.h>
#include <coap_option.h>
#include <lwm2m_api.h>
#include <lwm2m_remote.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>

// Set to 0 to skip bootstrap
#define BOOTSTRAP 1

// Set to 1 to connect to DM Server
// Set to 0 to connect to Repository Server
#define DM_SERVER 0

#define COAP_LOCAL_LISTENER_PORT              5683                                            /**< Local port to listen on any traffic, client or server. Not bound to any specific LWM2M functionality.*/
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     9998                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_LOCAL_CLIENT_PORT               9999                                            /**< Local port to connect to the LWM2M server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5684                                            /**< Remote port of the LWM2M bootstrap server. */
#define LWM2M_SERVER_REMORT_PORT              5684                                            /**< Remote port of the LWM2M server. */

#define BOOTSTRAP_URI                   "coaps://my.bootstrapserver.com:5684"                 /**< Server URI to the bootstrap server when using security (DTLS). */
#define CLIENT_IMEI_MSISDN              "urn:imei-msisdn:004402990020434-0123456789"          /**< IMEI-MSISDN of the device. */

#define LED_BLINK_INTERVAL_MS           30000                                                 /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS           50000                                                 /**< Interval between periodic callbacks to CoAP module. */

#define SECURITY_SERVER_URI_SIZE_MAX    64                                                    /**< Max size of server URIs. */

#define BOOTSTRAP_SECURITY_INSTANCE_IDX 0                                                     /**< Index of bootstrap security instance. */
#define SERVER_SECURITY_INSTANCE_IDX    1                                                     /**< Index of server security instance. */

#define APP_ENABLE_LOGS                 0                                                     /**< Enable logs in the application. */

#define APP_BOOTSTRAP_SEC_TAG           25                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_BOOTSTRAP_SEC_PSK           "d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747" /**< Pre-shared key used for bootstrap server in hex format. */
#define APP_BOOTSTRAP_SEC_IDENTITY      "urn:imei-msisdn:004402990020434-0123456789"          /**< Client identity used for bootstrap server. */

#define APP_SERVER_SEC_TAG              26                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#if (DM_SERVER == 1)
#define APP_SERVER_SEC_PSK              "a3676691ce05a04801d9b2b43e4a31db342d6428a4028e10d6c96ebbc07fd128" /**< Pre-shared key used for resource server in hex format. */
#else
#define APP_SERVER_SEC_PSK              "c16451b3c745dbd0b13b1daaf90b7f18da420ac0c344089f6cb8cb2f8e48f6fd" /**< Pre-shared key used for resource server in hex format. */
#endif
#define APP_SERVER_SEC_IDENTITY         "004402990020434"                                     /**< Client identity used for resource server. */

#define APP_MAX_AT_READ_LENGTH          100
#define APP_MAX_AT_WRITE_LENGTH         256

#if (APP_ENABLE_LOGS == 1)

#define APPL_LOG  NRF_LOG_INFO
#define APPL_DUMP NRF_LOG_RAW_HEXDUMP_INFO
#define APPL_ADDR IPV6_ADDRESS_LOG

#else // APP_ENABLE_LOGS

#define APPL_LOG(...)
#define APPL_DUMP(...)
#define APPL_ADDR(...)

#endif // APP_ENABLE_LOGS

#define APP_ERROR_CHECK(error_code) \
    do { \
        if (error_code != 0) { \
            printk("Error: %lu\n", error_code); \
            while (1) \
                ; \
        } \
    } while (0)

#define APP_ERROR_CHECK_BOOL(boolean_value) \
    do { \
        const uint32_t local_value = (boolean_value); \
        if (!local_value) { \
            printk("BOOL check failure\n"); \
            while (1) \
                ; \
        } \
    } while (0)

typedef enum
{
    APP_STATE_IDLE = 0,
    APP_STATE_IP_INTERFACE_UP,
    APP_STATE_BS_CONNECT,
    APP_STATE_BS_CONNECTED,
    APP_STATE_BOOTRAP_REQUESTED,
    APP_STATE_BOOTSTRAPED,
    APP_STATE_SERVER_CONNECT_INITIATE,
    APP_STATE_SERVER_CONNECTED,
    APP_STATE_SERVER_REGISTERED,
    APP_STATE_DISCONNECT
} app_state_t;

//APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                       /**< App timer instance used to update the IoT timer wall clock. */

static lwm2m_server_config_t               m_server_conf;                                     /**< Server configuration structure. */
static lwm2m_client_identity_t             m_client_id;                                       /**< Client ID structure to hold the client's UUID. */
static bool                                m_use_dtls[1+LWM2M_MAX_SERVERS] = {true, true};    /**< Array to keep track of which of the connections to bootstrap server and server is using a secure link. */
static uint8_t *                           mp_link_format_string    = NULL;                   /**< Pointer to hold a link format string across a button press initiated registration and retry. */
static uint32_t                            m_link_format_string_len = 0;                      /**< Length of the link format string that is used in registration attempts. */

// Objects
static lwm2m_object_t                      m_object_security;                                 /**< LWM2M security base object. */
static lwm2m_object_t                      m_object_server;                                   /**< LWM2M server base object. */
static lwm2m_object_t                      m_object_device;                                   /**< Device base object. */
static lwm2m_object_t                      m_object_conn_mon;                                 /**< Connectivity Monitoring base object. */
static lwm2m_object_t                      m_bootstrap_server;                                /**< Named object to be used as callback object when bootstrap is completed. */

static char m_bootstrap_object_alias_name[] = "bs";                                           /**< Name of the bootstrap complete object. */

// Instances
static lwm2m_security_t                    m_instance_security[1+LWM2M_MAX_SERVERS];          /**< Security object instances. Index 0 is always bootstrap instance. */
static lwm2m_server_t                      m_instance_server[1+LWM2M_MAX_SERVERS];            /**< Server object instance to be filled by the bootstrap server. */
static lwm2m_device_t                      m_instance_device;                                 /**< Device object instance. */
static lwm2m_connectivity_monitoring_t     m_instance_conn_mon;                               /**< Connectivity Monitoring object instance. */

static coap_transport_handle_t *           mp_coap_transport     = NULL;                      /**< CoAP transport handle for the non bootstrap server. */
static coap_transport_handle_t *           mp_lwm2m_bs_transport = NULL;                      /**< CoAP transport handle for the secure bootstrap server. Obtained on @coap_security_setup. */
static coap_transport_handle_t *           mp_lwm2m_transport    = NULL;                      /**< CoAP transport handle for the secure server. Obtained on @coap_security_setup. */

static volatile app_state_t m_app_state = APP_STATE_IDLE;                                     /**< Application state. Should be one of @ref app_state_t. */

static char m_at_write_buffer[APP_MAX_AT_WRITE_LENGTH];                                       /**< Buffer used to write AT commands. */
static char m_at_read_buffer[APP_MAX_AT_READ_LENGTH];                                         /**< Buffer used to read AT commands. */

#if defined(APP_USE_AF_INET6)

static struct sockaddr_in6 m_bs_server =
{
    .sin_port   = LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT,
    .sin_family = AF_INET6,
};

static struct sockaddr_in6 m_server =
{
    .sin_port   = LWM2M_SERVER_REMORT_PORT,
    .sin_family = AF_INET6,
};

#else //APP_USE_AF_INET6

static struct sockaddr_in m_bs_server =
{
    .sin_port        = htons(LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT),
    .sin_family      = AF_INET,
    .sin_addr.s_addr = htonl(0xCF4720E5) // 207.71.32.229
};

static struct sockaddr_in m_server =
{
    .sin_port        = htons(LWM2M_SERVER_REMORT_PORT),
    .sin_family      = AF_INET,
#if (DM_SERVER == 1)
    .sin_addr.s_addr = htonl(0xCF4720E6) // 207.71.32.230 = DM server
#else
    .sin_addr.s_addr = htonl(0xCF4720E7) // 207.71.32.231 = Repository server
#endif
};

#endif

static const struct sockaddr *   mp_bs_remote_server = (struct sockaddr *)&m_bs_server;       /**< Pointer to remote bootstrap server address to connect to. */
static const struct sockaddr *   mp_remote_server = (struct sockaddr *)&m_server;             /**< Pointer to remote secure server address to connect to. */

static void app_server_update(uint16_t instance_id);


/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

    while (true)
    {
        ;
    }
}


/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

    while (true)
    {
        ;
    }
}


/**@brief Callback for button events from the DK buttons and LEDs library. */
static void button_handler(u32_t buttons, u32_t has_changed)
{

}


/**@brief Initializes buttons and LEDs, using the DK buttons and LEDs library. */
static void buttons_leds_init(void)
{
    dk_buttons_and_leds_init(button_handler);
    dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
#if 0
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    ARG_UNUSED(wall_clock_value);
#if BUTTONS_AND_LEDS
    switch (m_app_state)
    {
        case APP_STATE_IDLE:
        {
            LEDS_ON((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
            break;
        }
        case APP_STATE_IP_INTERFACE_UP:
        {
            LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
            LEDS_ON(LED_ONE);
            break;
        }
        case APP_STATE_BS_CONNECT:
        {
            LEDS_INVERT(LED_TWO);
            break;
        }
        case APP_STATE_BOOTSTRAPED:
        {
            LEDS_ON(LED_TWO);
            break;
        }
        default:
        {
            break;
        }
    }
#endif
}


/**@brief Function for handling CoAP periodically time ticks.
*/
static void app_coap_time_tick(struct k_work *work)
{
    // Pass a tick to CoAP in order to re-transmit any pending messages.
    //(void)coap_time_tick();
}


/**@brief Function for updating the wall clock of the IoT Timer module.
 */
static void iot_timer_tick_callback(void * p_context)
{
    ARG_UNUSED(p_context);
    uint32_t err_code = iot_timer_update();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_ERROR_CHECK(app_timer_init());

    // Create a sys timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler,   LED_BLINK_INTERVAL_MS},
        {app_coap_time_tick,      COAP_TICK_INTERVAL_MS}
    };

    // The list of IoT Timer clients is declared as a constant.
    static const iot_timer_clients_list_t iot_timer_clients =
    {
        (sizeof(list_of_clients) / sizeof(iot_timer_client_t)),
        &(list_of_clients[0]),
    };

    // Passing the list of clients to the IoT Timer module.
    err_code = iot_timer_client_list_set(&iot_timer_clients);
    APP_ERROR_CHECK(err_code);

    // Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id,
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS),
                               NULL);
    APP_ERROR_CHECK(err_code);
}
#endif

/**@brief Application implementation of the root handler interface.
 *
 * @details This function is not bound to any object or instance. It will be called from
 *          LWM2M upon an action on the root "/" URI path. During bootstrap it is expected
 *          to get a DELETE operation on this URI.
 */
uint32_t lwm2m_coap_handler_root(uint8_t op_code, coap_message_t * p_request)
{
    (void)lwm2m_respond_with_code(COAP_CODE_202_DELETED, p_request);

    // Delete any existing objects or instances if needed.

    return 0;
}


/**@brief Helper function to parse the uri and save the remote to the LWM2M remote database. */
uint32_t lwm2m_parse_uri_and_save_remote(uint16_t short_server_id,
                                         char   * p_str,
                                         uint16_t str_len,
                                         bool   * p_use_dtls,
                                         const struct sockaddr * p_remote)
{
    uint32_t err_code;

    // Register the short_server_id
    err_code = lwm2m_remote_register(short_server_id);
    if (err_code != 0)
    {
        return err_code;
    }

    // Save the short_server_id
    err_code = lwm2m_remote_remote_save((struct sockaddr *)p_remote, short_server_id);

    if (err_code != 0)
    {
        return err_code;
    }

    return err_code;
}


/**@brief Helper function to get the access from an instance and a remote. */
uint32_t lwm2m_access_remote_get(uint16_t         * p_access,
                                 lwm2m_instance_t * p_instance,
                                 struct sockaddr  * p_remote)
{
    uint32_t err_code;
    uint16_t short_server_id;

    err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_remote);
    APP_ERROR_CHECK(err_code);

    err_code = lwm2m_acl_permissions_check(p_access, p_instance, short_server_id);

    // If we can't find the permission we return defaults.
    if (err_code != 0)
    {
        err_code = lwm2m_acl_permissions_check(p_access,
                                               p_instance,
                                               LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);
    }

    return err_code;
}


/**@brief LWM2M notification handler. */
void lwm2m_notification(lwm2m_notification_type_t type,
                        struct sockaddr *         p_remote,
                        uint8_t                   coap_code,
                        uint32_t                  err_code)
{
    APPL_LOG("Got LWM2M notifcation %d ", type);

    if (type == LWM2M_NOTIFCATION_TYPE_REGISTER)
    {
        // We have successfully registered, free up the allocated link format string.
        if (mp_link_format_string != NULL)
        {
            // No more attempts, clean up.
            k_free(mp_link_format_string);
            mp_link_format_string = NULL;
        }
    }
}


uint32_t lwm2m_handler_error(uint16_t           short_server_id,
                             lwm2m_instance_t * p_instance,
                             coap_message_t   * p_request,
                             uint32_t           err_code)
{
    // LWM2M will send an answer to the server based on the error code.
    return err_code;
}


/**@brief Callback function for the named bootstrap complete object. */
uint32_t bootstrap_object_callback(lwm2m_object_t * p_object,
                                   uint16_t         instance_id,
                                   uint8_t          op_code,
                                   coap_message_t * p_request)
{
    dk_set_leds(DK_LED3_MSK);

    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

    m_app_state = APP_STATE_BOOTSTRAPED;

    return 0;
}


/**@brief Callback function for LWM2M server instances. */
uint32_t server_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t *   p_request)
{
    APPL_LOG("lwm2m: server_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_remote_get(&access,
                                                p_instance,
                                                p_request->p_remote);
    APP_ERROR_CHECK(err_code);

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        err_code = lwm2m_tlv_server_encode(buffer,
                                           &buffer_size,
                                           resource_id,
                                           &m_instance_server[instance_id]);

        if (err_code == ENOENT)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return 0;
        }

        APP_ERROR_CHECK(err_code);

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

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                               p_request->p_payload,
                                               p_request->payload_len);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_server_decode(&m_instance_server[instance_id],
                                                      resource_id,
                                                      p_request->p_payload,
                                                      p_request->payload_len);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_request);
            return 0;
        }

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
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
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_SERVER_DISABLE:
            {
                // TODO: Disconnect, wait disable_timeout seconds and reconnect.
                (void)lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
                break;
            }

            case LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
                app_server_update(instance_id);
                break;
            }

            default:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                return 0;
            }
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}


/**@brief Callback function for device instances. */
uint32_t device_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t   * p_request)
{
    APPL_LOG("lwm2m: device_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_remote_get(&access,
                                                p_instance,
                                                p_request->p_remote);
    APP_ERROR_CHECK(err_code);

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        err_code = lwm2m_tlv_device_encode(buffer,
                                           &buffer_size,
                                           resource_id,
                                           &m_instance_device);

        if (err_code == ENOENT)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return 0;
        }

        APP_ERROR_CHECK(err_code);

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

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_device_decode(&m_instance_device,
                                               p_request->p_payload,
                                               p_request->payload_len);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_device_decode(&m_instance_device,
                                                      resource_id,
                                                      p_request->p_payload,
                                                      p_request->payload_len);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_request);
            return 0;
        }

        if (err_code == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
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
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_DEVICE_REBOOT:
            case LWM2M_DEVICE_FACTORY_RESET:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                // TODO: Shutdown and reboot
                NVIC_SystemReset();
                break;
            }

            case LWM2M_DEVICE_RESET_ERROR_CODE:
            {
                m_instance_device.error_code.len = 1;
                m_instance_device.error_code.val.p_int32[0] = 0;

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
                break;
            }

            default:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                return 0;
            }
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}


/**@brief Callback function for connectivity_monitoring instances. */
uint32_t conn_mon_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    APPL_LOG("lwm2m: conn_mon_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_remote_get(&access,
                                                p_instance,
                                                p_request->p_remote);
    APP_ERROR_CHECK(err_code);

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                            &buffer_size,
                                                            resource_id,
                                                            &m_instance_conn_mon);
        if (err_code == ENOENT)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return 0;
        }

        APP_ERROR_CHECK(err_code);

        (void)lwm2m_respond_with_payload(buffer, buffer_size, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}


/**@brief Callback function for LWM2M server objects. */
uint32_t server_object_callback(lwm2m_object_t * p_object,
                                uint16_t         instance_id,
                                uint8_t          op_code,
                                coap_message_t * p_request)
{
    APPL_LOG("lwm2m: server_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        // Remember to make a copy of lwm2m_string_t and lwm2m_opaque_t you want to keep.
        // They will be freed after this callback.
        (void)lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                      p_request->p_payload,
                                      p_request->payload_len);



        m_instance_server[instance_id].proto.instance_id = instance_id;
        m_instance_server[instance_id].proto.object_id   = p_object->object_id;
        m_instance_server[instance_id].proto.callback    = server_instance_callback;

        // Cast the instance to its prototype and add it.
        (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_server[instance_id]);
        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[instance_id]);

        // Initialize ACL on the instance
        // The owner (second parameter) is set to LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID.
        // This will grant the Bootstrap server full permission to this instance.
        (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[instance_id],
                                         LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

        (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}


/**@brief Callback function for LWM2M security instances. */
uint32_t security_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    APPL_LOG("lwm2m: security_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = lwm2m_access_remote_get(&access,
                                                p_instance,
                                                p_request->p_remote);
    APP_ERROR_CHECK(err_code);

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint16_t instance_id = p_instance->instance_id;

        // Remember to make a copy of lwm2m_string_t and lwm2m_opaque_t you want to keep.
        // They will be freed after this callback
        err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                             p_request->p_payload,
                                             p_request->payload_len);
        APP_ERROR_CHECK(err_code);

        // Copy the URI. Can be changed to handle all resources in the instance.
        if (m_instance_security[instance_id].server_uri.len < SECURITY_SERVER_URI_SIZE_MAX)
        {
            // Parse URI to remote and save it.
            err_code = lwm2m_parse_uri_and_save_remote(m_instance_security[instance_id].short_server_id,
                                                       m_instance_security[instance_id].server_uri.p_val,
                                                       m_instance_security[instance_id].server_uri.len,
                                                       &m_use_dtls[instance_id],
                                                       p_request->p_remote);
            APP_ERROR_CHECK(err_code);

            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            // URI was to long to be copied.
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return 0;
}


/**@brief Callback function for LWM2M object instances. */
uint32_t security_object_callback(lwm2m_object_t  * p_object,
                                  uint16_t          instance_id,
                                  uint8_t           op_code,
                                  coap_message_t *  p_request)
{
    APPL_LOG("lwm2m: security_object_callback, instance %u", instance_id);

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        // Remember to make a copy of lwm2m_string_t and lwm2m_opaque_t you want to keep.
        // They will be freed after this callback
        uint32_t err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                                      p_request->p_payload,
                                                      p_request->payload_len);
        APP_ERROR_CHECK(err_code);

        // FIXME: Provision keys instead of print.
        printk("Secret Key %u: ", instance_id);
        for (int i = 0; i < m_instance_security[instance_id].secret_key.len; i++) {
            printk("%02x", m_instance_security[instance_id].secret_key.p_val[i]);
        }
        printk("\n");

        APPL_LOG("lwm2m: decoded security.");

        m_instance_security[instance_id].proto.instance_id = instance_id;
        m_instance_security[instance_id].proto.object_id   = p_object->object_id;
        m_instance_security[instance_id].proto.callback    = security_instance_callback;

        if (m_instance_security[instance_id].server_uri.len < SECURITY_SERVER_URI_SIZE_MAX)
        {
            // No ACL object for security objects.
            ((lwm2m_instance_t *)&m_instance_security[instance_id])->acl.id = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;

            // Cast the instance to its prototype and add it to the CoAP handler to become a
            // public instance. We can only have one so we delete the first if any.
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[instance_id]);

            (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_security[instance_id]);

            // Parse URI to remote and save it
            err_code = lwm2m_parse_uri_and_save_remote(m_instance_security[instance_id].short_server_id,
                                                       m_instance_security[instance_id].server_uri.p_val,
                                                       m_instance_security[instance_id].server_uri.len,
                                                       &m_use_dtls[instance_id],
                                                       mp_bs_remote_server);

            APP_ERROR_CHECK(err_code);

            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
            // URI was too long to be copied.
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return 0;
}


static void app_lwm2m_factory_bootstrap(void)
{
    char * binding_uqs = "UQS";
    char * binding_uq  = "UQ";

    //
    // Bootstrap server instance.
    //
    m_instance_server[0].short_server_id = 100;

    m_instance_server[0].proto.callback = server_instance_callback;

    // FIXME: Fix the ACL issue
#if 0
    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[0],
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    // Set server access.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[0],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);
#endif

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[0]);

#if (BOOTSTRAP == 0)
    // Register the short_server_id.
    (void)lwm2m_remote_register(m_instance_server[0].short_server_id);

    // Save the short_server_id.
    (void)lwm2m_remote_remote_save((struct sockaddr *)mp_bs_remote_server, m_instance_server[0].short_server_id);
#endif

    //
    // DM server instance.
    //
    m_instance_server[1].short_server_id                  = 102;
    m_instance_server[1].lifetime                         = 2592000;
    m_instance_server[1].default_minimum_period           = 1;
    m_instance_server[1].default_maximum_period           = 60;
    m_instance_server[1].disable_timeout                  = 86400;
    m_instance_server[1].notification_storing_on_disabled = 1;
    (void)lwm2m_bytebuffer_to_string(binding_uqs, strlen(binding_uqs), &m_instance_server[1].binding);

    m_instance_server[1].proto.callback = server_instance_callback;

    // Initialize ACL on the instance.
#if (BOOTSTRAP == 0)
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[1],
                                     102);
#else
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[1],
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
#endif

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[1],
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    // Set server access.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[1],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    101);
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[1],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[1],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[1]);

    // FIXME: Init ACL for bootstrap server here to make DM server[1] get ACL /2/0 which is according to Verizon spec
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[0],
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[0],
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    // Set server access.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[0],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);

#if (BOOTSTRAP == 0) && (DM_SERVER == 1)
    // Register the short_server_id.
    (void)lwm2m_remote_register(m_instance_server[1].short_server_id);

    // Save the short_server_id.
    (void)lwm2m_remote_remote_save((struct sockaddr *)mp_remote_server, m_instance_server[1].short_server_id);
#endif

    //
    // Diagnostics server instance.
    //
    m_instance_server[2].short_server_id                  = 101;
    m_instance_server[2].lifetime                         = 86400;
    m_instance_server[2].default_minimum_period           = 300;
    m_instance_server[2].default_maximum_period           = 6000;
    m_instance_server[2].notification_storing_on_disabled = 1;
    (void)lwm2m_bytebuffer_to_string(binding_uqs, strlen(binding_uqs), &m_instance_server[2].binding);

    m_instance_server[2].proto.callback = server_instance_callback;

    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[2],
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[2],
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    // Set server access.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[2],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[2]);

    //
    // Repository server instance.
    //
    m_instance_server[3].short_server_id                  = 1000;
    m_instance_server[3].lifetime                         = 86400;
    m_instance_server[3].default_minimum_period           = 1;
    m_instance_server[3].default_maximum_period           = 6000;
    m_instance_server[3].disable_timeout                  = 86400;
    m_instance_server[3].notification_storing_on_disabled = 1;
    (void)lwm2m_bytebuffer_to_string(binding_uq, strlen(binding_uq), &m_instance_server[3].binding);

    m_instance_server[3].proto.callback = server_instance_callback;

    // Initialize ACL on the instance.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[3],
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[3],
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    // Set server access.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[3],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    101);
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[3],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[3],
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[3]);

#if (BOOTSTRAP == 0) && (DM_SERVER == 0)
    // Register the short_server_id.
    (void)lwm2m_remote_register(m_instance_server[3].short_server_id);

    // Save the short_server_id.
    (void)lwm2m_remote_remote_save((struct sockaddr *)mp_remote_server, m_instance_server[3].short_server_id);
#endif

    //
    // Device instance.
    //
    lwm2m_instance_device_init(&m_instance_device);

    m_instance_device.manufacturer.p_val = "Nordic Semiconductor";
    m_instance_device.manufacturer.len = strlen(m_instance_device.manufacturer.p_val);
    m_instance_device.model_number.p_val = "nRF91";
    m_instance_device.model_number.len = strlen(m_instance_device.model_number.p_val);
    m_instance_device.serial_number.p_val = "1234567890";
    m_instance_device.serial_number.len = strlen(m_instance_device.serial_number.p_val);
    m_instance_device.firmware_version.p_val = "1.0";
    m_instance_device.firmware_version.len = strlen(m_instance_device.firmware_version.p_val);
    m_instance_device.avail_power_sources.len = 3;
    m_instance_device.avail_power_sources.val.p_uint8[0] = 0; // DC power
    m_instance_device.avail_power_sources.val.p_uint8[1] = 2; // External Battery
    m_instance_device.avail_power_sources.val.p_uint8[2] = 5; // USB
    m_instance_device.power_source_voltage.len = 3;
    m_instance_device.power_source_voltage.val.p_int32[0] = 5108;
    m_instance_device.power_source_voltage.val.p_int32[1] = 5242;
    m_instance_device.power_source_voltage.val.p_int32[2] = 5000;
    m_instance_device.power_source_current.len = 3;
    m_instance_device.power_source_current.val.p_int32[0] = 42;
    m_instance_device.power_source_current.val.p_int32[1] = 0;
    m_instance_device.power_source_current.val.p_int32[2] = 0;
    m_instance_device.battery_level = 20;
    m_instance_device.memory_free = 40;
    m_instance_device.error_code.len = 1;
    m_instance_device.error_code.val.p_int32[0] = 1; // Low battery power
    m_instance_device.current_time = 1536738518; // Wed Sep 12 09:48:38 CEST 2018
    char * utc_offset = "+02:00";
    (void)lwm2m_bytebuffer_to_string(utc_offset, strlen(utc_offset), &m_instance_device.utc_offset);
    char * timezone = "Europe/Oslo";
    (void)lwm2m_bytebuffer_to_string(timezone, strlen(timezone),&m_instance_device.timezone);
    (void)lwm2m_bytebuffer_to_string(binding_uqs, strlen(binding_uqs), &m_instance_device.supported_bindings);
    m_instance_device.device_type.p_val = "Smart Device";
    m_instance_device.device_type.len = strlen(m_instance_device.device_type.p_val);
    m_instance_device.hardware_version.p_val = "1.0";
    m_instance_device.hardware_version.len = strlen(m_instance_device.hardware_version.p_val);
    m_instance_device.software_version.p_val = "1.0";
    m_instance_device.software_version.len = strlen(m_instance_device.software_version.p_val);
    m_instance_device.battery_status = 4;
    m_instance_device.memory_total = 128;

    m_instance_device.proto.callback = device_instance_callback;

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_device,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_device);

    //
    // Connectivity Monitoring instance.
    //
    lwm2m_instance_connectivity_monitoring_init(&m_instance_conn_mon);

    m_instance_conn_mon.network_bearer = 6;
    m_instance_conn_mon.available_network_bearer.len = 2;
    m_instance_conn_mon.available_network_bearer.val.p_int32[0] = 5;
    m_instance_conn_mon.available_network_bearer.val.p_int32[1] = 6;
    m_instance_conn_mon.radio_signal_strength = 42;
    m_instance_conn_mon.link_quality = 100;
    m_instance_conn_mon.ip_addresses.len = 1;
    char * ip_address = "192.168.0.0";
    (void)lwm2m_bytebuffer_to_string(ip_address, strlen(ip_address), &m_instance_conn_mon.ip_addresses.val.p_string[0]);
    m_instance_conn_mon.link_utilization = 100;
    m_instance_conn_mon.apn.len = 1;
    char * apn = "VZWADMIN";
    (void)lwm2m_bytebuffer_to_string(apn, strlen(apn), &m_instance_conn_mon.apn.val.p_string[0]);
    m_instance_conn_mon.cell_id = 0;
    m_instance_conn_mon.smnc = 1;
    m_instance_conn_mon.smcc = 1;

    m_instance_conn_mon.proto.callback = conn_mon_instance_callback;

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_conn_mon,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE),
                                    102);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_conn_mon);
}


/**@brief LWM2M initialization.
 *
 * @details The function will register all implemented base objects as well as initial registration
 *          of existing instances. If bootstrap is not performed, the registration to the server
 *          will use what is initialized in this function.
 */
static void lwm2m_setup(void)
{
    (void)lwm2m_init(k_malloc, k_free);
    (void)lwm2m_remote_init();
    (void)lwm2m_acl_init();

    // Save the remote address of the bootstrap server.
    (void)lwm2m_parse_uri_and_save_remote(LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID,
                                          BOOTSTRAP_URI,
                                          (uint16_t)strlen(BOOTSTRAP_URI),
                                          &m_use_dtls[BOOTSTRAP_SECURITY_INSTANCE_IDX],
                                          mp_bs_remote_server);

    m_bootstrap_server.object_id    = LWM2M_NAMED_OBJECT;
    m_bootstrap_server.callback     = bootstrap_object_callback;
    m_bootstrap_server.p_alias_name = m_bootstrap_object_alias_name;
    (void)lwm2m_coap_handler_object_add(&m_bootstrap_server);

    // Add security support.
    m_object_security.object_id = LWM2M_OBJ_SECURITY;
    m_object_security.callback = security_object_callback;
    (void)lwm2m_coap_handler_object_add(&m_object_security);

    // Add server support.
    m_object_server.object_id = LWM2M_OBJ_SERVER;
    m_object_server.callback = server_object_callback;
    (void)lwm2m_coap_handler_object_add(&m_object_server);

    // Add device support.
    m_object_device.object_id = LWM2M_OBJ_DEVICE;
    //m_object_device.callback = device_object_callback;
    (void)lwm2m_coap_handler_object_add(&m_object_device);

    // Add connectivity monitoring support.
    m_object_conn_mon.object_id = LWM2M_OBJ_CONN_MON;
    //m_object_conn_mon.callback = device_object_callback;
    (void)lwm2m_coap_handler_object_add(&m_object_conn_mon);

    // Initialize the instances.
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        lwm2m_instance_security_init(&m_instance_security[i]);
        m_instance_security[i].proto.instance_id = i;

        lwm2m_instance_server_init(&m_instance_server[i]);
        m_instance_server[i].proto.instance_id = i;
    }

    // Set client ID.
    memcpy(&m_client_id.value.imei_msisdn[0], CLIENT_IMEI_MSISDN, LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN);
    m_client_id.type = LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN;
}


static void app_bootstrap_connect(void)
{
    uint32_t err_code;

    if (m_use_dtls[BOOTSTRAP_SECURITY_INSTANCE_IDX] == true)
    {
        APPL_LOG("SECURE session (bootstrap)");

        #if defined(APP_USE_AF_INET6)

            const struct sockaddr_in client_addr =
            {
                .sin6_port   = htons(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin6_family = AF_INET6,
                .sin
            };

        #else // APP_USE_AF_INET6

            const struct sockaddr_in client_addr =
            {
                .sin_port        = htons(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin_family      = AF_INET,
                .sin_addr.s_addr = 0
            };

        #endif // APP_USE_AF_INET6

            #define SEC_TAG_COUNT 1

            struct sockaddr * p_localaddr = (struct sockaddr *)&client_addr;

            sec_tag_t sec_tag_list[SEC_TAG_COUNT] = {APP_BOOTSTRAP_SEC_TAG};

            coap_sec_config_t setting =
            {
                .role           = 0,    // 0 -> Client role
                .sec_tag_count  = SEC_TAG_COUNT,
                .p_sec_tag_list = sec_tag_list
            };


            coap_local_t local_port =
            {
                .p_addr    = p_localaddr,
                .p_setting = &setting,
                .protocol  = IPPROTO_DTLS_1_2
            };

            // NOTE: This method initiates a DTLS handshake and may block for a some seconds.
            err_code = coap_security_setup(&local_port, mp_bs_remote_server);

            if (err_code == 0)
            {
                mp_lwm2m_bs_transport = local_port.p_transport;
                m_app_state = APP_STATE_BS_CONNECTED;
            }
    }
    else
    {
        APPL_LOG("NON-SECURE session (bootstrap)");
    }
}


static void app_bootstrap(void)
{
    uint32_t err_code = lwm2m_bootstrap((struct sockaddr *)mp_bs_remote_server, &m_client_id, mp_lwm2m_bs_transport);
    if (err_code == 0)
    {
        m_app_state = APP_STATE_BOOTRAP_REQUESTED;
    }
}


static void app_server_connect(void)
{
    uint32_t err_code;

    // Initialize server configuration structure.
    memset(&m_server_conf, 0, sizeof(lwm2m_server_config_t));
    m_server_conf.lifetime = 1000;

    // Set the short server id of the server in the config.
    m_server_conf.short_server_id = m_instance_security[SERVER_SECURITY_INSTANCE_IDX].short_server_id;

    if (m_use_dtls[SERVER_SECURITY_INSTANCE_IDX] == true)
    {
        APPL_LOG("SECURE session (register)");
    #if defined(APP_USE_AF_INET6)

        const struct sockaddr_in client_addr =
        {
            .sin6_port   = htons(LWM2M_LOCAL_CLIENT_PORT)
            .sin6_family = htons(AF_INET6),
            .sin
        };

    #else // APP_USE_AF_INET6

        const struct sockaddr_in client_addr =
        {
            .sin_port        = htons(LWM2M_LOCAL_CLIENT_PORT),
            .sin_family      = AF_INET,
            .sin_addr.s_addr = 0
        };

    #endif // APP_USE_AF_INET6

        #define SEC_TAG_COUNT 1

        struct sockaddr * p_localaddr = (struct sockaddr *)&client_addr;

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = {APP_SERVER_SEC_TAG};

        coap_sec_config_t setting =
        {
            .role           = 0,    // 0 -> Client role
            .sec_tag_count  = SEC_TAG_COUNT,
            .p_sec_tag_list = sec_tag_list
        };


        coap_local_t local_port =
        {
            .p_addr    = p_localaddr,
            .p_setting = &setting,
            .protocol  = IPPROTO_DTLS_1_2
        };

        // NOTE: This method initiates a DTLS handshake and may block for some seconds.
        err_code = coap_security_setup(&local_port, mp_remote_server);

        if (err_code == 0)
        {
            mp_lwm2m_transport = local_port.p_transport;

            m_app_state = APP_STATE_SERVER_CONNECTED;
        }
    }
    else
    {
        APPL_LOG("NON-SECURE session (register)");
    }
}


static void app_server_register(void)
{
    uint32_t err_code;

    // Dry run the link format generation, to check how much memory that is needed.
    err_code = lwm2m_coap_handler_gen_link_format(NULL, (uint16_t *)&m_link_format_string_len);
    APP_ERROR_CHECK(err_code);

    // Allocate the needed amount of memory.
    mp_link_format_string = k_malloc(m_link_format_string_len);

    if (mp_link_format_string != NULL)
    {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(mp_link_format_string, (uint16_t *)&m_link_format_string_len);
        APP_ERROR_CHECK(err_code);

        err_code = lwm2m_register((struct sockaddr *)mp_remote_server,
                                  &m_client_id,
                                  &m_server_conf,
                                  mp_lwm2m_transport,
                                  mp_link_format_string,
                                  (uint16_t)m_link_format_string_len);

        if (err_code == 0)
        {
            m_app_state = APP_STATE_SERVER_REGISTERED;
            dk_set_leds(DK_LED4_MSK);
        }
    }
}


static void app_server_update(uint16_t instance_id)
{
    uint32_t err_code;

    // TODO: check instance_id
    ARG_UNUSED(instance_id);

    err_code = lwm2m_update((struct sockaddr *)mp_remote_server,
                            &m_server_conf,
                            mp_lwm2m_transport);
    APP_ERROR_CHECK(err_code);
}


static void app_disconnect(void)
{
    uint32_t err_code;

    // Destroy the secure session if any.
    if (m_use_dtls[BOOTSTRAP_SECURITY_INSTANCE_IDX] == true)
    {
        err_code = coap_security_destroy(mp_lwm2m_bs_transport);
        APP_ERROR_CHECK(err_code);
    }

    if (m_use_dtls[SERVER_SECURITY_INSTANCE_IDX] == true)
    {
        err_code = coap_security_destroy(mp_lwm2m_transport);
        APP_ERROR_CHECK(err_code);
    }

    m_app_state = APP_STATE_IP_INTERFACE_UP;
}


void app_lwm2m_process(void)
{
    coap_input();

    switch(m_app_state)
    {
        case APP_STATE_BS_CONNECT:
        {
            printk("app_bootstrap_connect()\n");
            app_bootstrap_connect();
            break;
        }
        case APP_STATE_BS_CONNECTED:
        {
            dk_set_leds(DK_LED2_MSK);
            printk("app_bootstrap()\n");
            app_bootstrap();
            break;
        }
        case APP_STATE_BOOTSTRAPED:
        {
            printk("app_server_connect()\n");
            app_server_connect();
            break;
        }
        case APP_STATE_SERVER_CONNECTED:
        {
            printk("app_server_register()\n");
            app_server_register();
            break;
        }
        case APP_STATE_DISCONNECT:
        {
            printk("app_disconnect()\n");
            app_disconnect();
            break;
        }
        default:
        {
            break;
        }
    }
}

void app_coap_init(void)
{
    uint32_t err_code;

    #if defined(APP_USE_AF_INET6)
    struct sockaddr_in6 local_client_addr =
    {
        .sin6_port   = htons(COAP_LOCAL_LISTENER_PORT),
        .sin6_family = AF_INET6,
    };
    #else // APP_USE_AF_INET6
    struct sockaddr_in local_client_addr =
    {
        .sin_port   = htons(COAP_LOCAL_LISTENER_PORT),
        .sin_family = AF_INET,
    };
    #endif // APP_USE_AF_INET6

    // If bootstrap server and server is using different port we can
    // register the ports individually.
    coap_local_t local_port_list[COAP_PORT_COUNT] =
    {
        {
            .p_addr = (struct sockaddr *)&local_client_addr
        }
    };

    // Verify that the port count defined in sdk_config.h is matching the one configured for coap_init.
    APP_ERROR_CHECK_BOOL(((sizeof(local_port_list)) / (sizeof(coap_local_t))) == COAP_PORT_COUNT);

    coap_transport_init_t port_list;
    port_list.p_port_table = &local_port_list[0];

    err_code = coap_init(17, &port_list, k_malloc, k_free);
    APP_ERROR_CHECK(err_code);

    mp_coap_transport = local_port_list[0].p_transport;
    ARG_UNUSED(mp_coap_transport);
}


/**@brief Function to provision credentials used for secure transport by the CoAP client. */
static void app_provision(void)
{
    int at_socket_fd  = -1;
    int bytes_written = 0;
    int bytes_read    = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    APP_ERROR_CHECK_BOOL(at_socket_fd >= 0);

    #define WRITE_OPCODE  0
    #define IDENTITY_CODE 4
    #define PSK_CODE      3

    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_BOOTSTRAP_SEC_TAG,
                                 IDENTITY_CODE,
                                 APP_BOOTSTRAP_SEC_IDENTITY);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_BOOTSTRAP_SEC_TAG,
                                 PSK_CODE,
                                 APP_BOOTSTRAP_SEC_PSK);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);


    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_SERVER_SEC_TAG,
                                 IDENTITY_CODE,
                                 APP_SERVER_SEC_IDENTITY);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_SERVER_SEC_TAG,
                                 PSK_CODE,
                                 APP_SERVER_SEC_PSK);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    ARG_UNUSED(close(at_socket_fd));
}


/**@brief Function to configure the modem and create a LTE connection. */
static void app_modem_configure(void)
{
    int at_socket_fd  = -1;
    int bytes_written = 0;
    int bytes_read    = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    APP_ERROR_CHECK_BOOL(at_socket_fd >= 0);

    bytes_written = send(at_socket_fd, "AT+CEREG=2", 10, 0);
    APP_ERROR_CHECK_BOOL(bytes_written == 10);

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    bytes_written = send(at_socket_fd, "AT+CFUN=1", 9, 0);
    APP_ERROR_CHECK_BOOL(bytes_written == 9);

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    while (true)
    {
        bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);

        if ((strncmp("+CEREG: 1", m_at_read_buffer, 9) == 0) ||
            (strncmp("+CEREG:1", m_at_read_buffer, 8) == 0) ||
            (strncmp("+CEREG: 5", m_at_read_buffer, 9) == 0) ||
            (strncmp("+CEREG:5", m_at_read_buffer, 8) == 0))
        {
            break;
        }
    }
    ARG_UNUSED(close(at_socket_fd));

#if (BOOTSTRAP == 1)
    m_app_state = APP_STATE_BS_CONNECT;
#else
    m_app_state = APP_STATE_BOOTSTRAPED;
#endif
}


/**@brief Function for application main entry.
 */
int main(void)
{
    printk("Application started\n");

    // Provision credentials used for the bootstrap server.
    app_provision();

    // Initialize CoAP.
    app_coap_init();

    // Setup LWM2M endpoints.
    lwm2m_setup();

    // Establish LTE link.
    app_modem_configure();
    //lte_lc_init_and_connect();

    // Initialize LEDs and Buttons.
    buttons_leds_init();

    // Initialize Timers.
    //timers_init();
    //iot_timer_init();

    // Create LwM2M factory bootstraped objects.
    app_lwm2m_factory_bootstrap();

    dk_set_leds(DK_LED1_MSK);

    // Enter main loop
    for (;;)
    {
        app_lwm2m_process();
    }
}

/**
 * @}
 */
