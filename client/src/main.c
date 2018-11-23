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
#include <nvs/nvs.h>
//#include <lte_lc.h>
#include <nrf.h>

#include <coap_api.h>
#include <coap_option.h>
#include <lwm2m_api.h>
#include <lwm2m_remote.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>

/* Hardcoded IMEI for now, will be fetched from modem using AT+CGSN=1 */
#define IMEI            "004402990020434"

#define APP_ACL_DM_SERVER_HACK          1
#define APP_USE_BUTTONS_AND_LEDS        1
#define APP_USE_AF_INET6                0
#define APP_USE_NVS                     0

#if APP_USE_BUTTONS_AND_LEDS
#include <dk_buttons_and_leds.h>

/* Structure for delayed work */
static struct k_delayed_work leds_update_work;
#endif

#define LED_ON(x)                       (x)
#define LED_BLINK(x)                    ((x) << 8)
#define LED_INDEX(x)                    ((x) << 16)
#define LED_GET_ON(x)                   ((x) & 0xFF)
#define LED_GET_BLINK(x)                (((x) >> 8) & 0xFF)

/* Interval in milliseconds between each time status LEDs are updated. */
#define APP_LEDS_UPDATE_INTERVAL        500

#define COAP_LOCAL_LISTENER_PORT              5683                                            /**< Local port to listen on any traffic, client or server. Not bound to any specific LWM2M functionality.*/
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     9998                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_LOCAL_CLIENT_PORT               9999                                            /**< Local port to connect to the LWM2M server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5684                                            /**< Remote port of the LWM2M bootstrap server. */
#define LWM2M_SERVER_REMORT_PORT              5684                                            /**< Remote port of the LWM2M server. */

#define BOOTSTRAP_URI                   "coaps://ddocdpboot.do.motive.com:5684"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define CLIENT_IMEI_MSISDN              "urn:imei-msisdn:" IMEI "-0123456789"                 /**< IMEI-MSISDN of the device. */

#define LED_BLINK_INTERVAL_MS           30000                                                 /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS           50000                                                 /**< Interval between periodic callbacks to CoAP module. */

#define SECURITY_SERVER_URI_SIZE_MAX    64                                                    /**< Max size of server URIs. */
#define SECURITY_SMS_NUMBER_SIZE_MAX    20                                                    /**< Max size of server SMS number. */
#define SERVER_BINDING_SIZE_MAX         4                                                     /**< Max size of server binding. */

#define APP_ENABLE_LOGS                 0                                                     /**< Enable logs in the application. */

#define APP_SEC_TAG_OFFSET              25

#define APP_BOOTSTRAP_SEC_TAG           25                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_BOOTSTRAP_SEC_PSK           "d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747" /**< Pre-shared key used for bootstrap server in hex format. */
#define APP_BOOTSTRAP_SEC_IDENTITY      CLIENT_IMEI_MSISDN                                    /**< Client identity used for bootstrap server. */

#if 0
#define APP_DM_SERVER_SEC_TAG           26                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_DM_SERVER_SEC_PSK           "ea61b935048a556a99590ac6f5ace87d18e68a88504dd10bec6a9caeefc8c975" /**< Pre-shared key used for resource server in hex format. */
#define APP_DM_SERVER_SEC_IDENTITY      IMEI                                                  /**< Client identity used for resource server. */

//      APP_DIAG_SERVER_SEC_TAG         27

#define APP_RS_SERVER_SEC_TAG           28                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_RS_SERVER_SEC_PSK           "c16451b3c745dbd0b13b1daaf90b7f18da420ac0c344089f6cb8cb2f8e48f6fd" /**< Pre-shared key used for resource server in hex format. */
#define APP_RS_SERVER_SEC_IDENTITY      IMEI                                                  /**< Client identity used for resource server. */
#endif

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
    APP_STATE_IDLE                    = LED_BLINK(DK_LED1_MSK),
    APP_STATE_IP_INTERFACE_UP         = LED_ON(DK_LED1_MSK),
    APP_STATE_BS_CONNECT              = LED_BLINK(DK_LED1_MSK) | LED_BLINK(DK_LED2_MSK),
    APP_STATE_BS_CONNECTED            = LED_ON(DK_LED1_MSK) | LED_BLINK(DK_LED2_MSK),
    APP_STATE_BOOTRAP_REQUESTED       = LED_ON(DK_LED1_MSK) | LED_BLINK(DK_LED2_MSK) | LED_INDEX(1),
    APP_STATE_BOOTSTRAPED             = LED_BLINK(DK_LED1_MSK) | LED_BLINK(DK_LED3_MSK),
    APP_STATE_SERVER_CONNECTED        = LED_ON(DK_LED1_MSK) | LED_BLINK(DK_LED3_MSK),
    APP_STATE_SERVER_REGISTERED       = LED_ON(DK_LED1_MSK) | LED_ON(DK_LED3_MSK),
    APP_STATE_SERVER_DEREGISTER       = LED_BLINK(DK_LED1_MSK) | LED_ON(DK_LED3_MSK),
    APP_STATE_SERVER_DEREGISTERING    = LED_BLINK(DK_LED1_MSK) | LED_ON(DK_LED3_MSK) | LED_INDEX(1),
    APP_STATE_DISCONNECT              = LED_BLINK(DK_LED1_MSK) | LED_ON(DK_LED3_MSK) | LED_INDEX(2)
} app_state_t;

//APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                       /**< App timer instance used to update the IoT timer wall clock. */

static lwm2m_server_config_t               m_server_conf;                                     /**< Server configuration structure. */
static lwm2m_client_identity_t             m_client_id;                                       /**< Client ID structure to hold the client's UUID. */
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

#define VERIZON_RESOURCE 30000

static int32_t                             m_security_hold_off_timer[1+LWM2M_MAX_SERVERS];
static int32_t                             m_security_is_bootstrapped[1+LWM2M_MAX_SERVERS];
static int32_t                             m_server_is_registered[1+LWM2M_MAX_SERVERS];
static int32_t                             m_server_client_hold_off_timer[1+LWM2M_MAX_SERVERS];
static lwm2m_string_t                      m_apn[4];                                          /**< Verizon specific APN names. */

static coap_transport_handle_t *           mp_coap_transport     = NULL;                      /**< CoAP transport handle for the non bootstrap server. */
static coap_transport_handle_t *           mp_lwm2m_bs_transport = NULL;                      /**< CoAP transport handle for the secure bootstrap server. Obtained on @coap_security_setup. */
static coap_transport_handle_t *           mp_lwm2m_transport    = NULL;                      /**< CoAP transport handle for the secure server. Obtained on @coap_security_setup. */

static volatile app_state_t m_app_state = APP_STATE_IDLE;                                     /**< Application state. Should be one of @ref app_state_t. */
static volatile uint16_t    m_server_instance;
static volatile bool        m_did_bootstrap;

static char *               m_public_key[1+LWM2M_MAX_SERVERS];
static char *               m_secret_key[1+LWM2M_MAX_SERVERS];

static char m_at_write_buffer[APP_MAX_AT_WRITE_LENGTH];                                       /**< Buffer used to write AT commands. */
static char m_at_read_buffer[APP_MAX_AT_READ_LENGTH];                                         /**< Buffer used to read AT commands. */

#if (APP_USE_AF_INET6 == 1)
static struct sockaddr_in6 m_bs_server;
static struct sockaddr_in6 m_server;
#else // APP_USE_AF_INET6
static struct sockaddr_in m_bs_server;
static struct sockaddr_in m_server;
#endif // APP_USE_AF_INET6

static struct sockaddr * mp_bs_remote_server = (struct sockaddr *)&m_bs_server;               /**< Pointer to remote bootstrap server address to connect to. */
static struct sockaddr * mp_remote_server = (struct sockaddr *)&m_server;                     /**< Pointer to remote secure server address to connect to. */

/**@brief Bootstrap values to store in app persistent storage. */
typedef struct
{
    uint16_t short_server_id;

    // Security object values
    char     server_uri[SECURITY_SERVER_URI_SIZE_MAX];                 /**< Server URI to the server. */
    bool     is_bootstrap_server;
    uint8_t  sms_security_mode;
    //char     sms_binding_key_param[6];
    //char     sms_binding_secret_keys[48];
    char     sms_number[SECURITY_SMS_NUMBER_SIZE_MAX];
    time_t   client_hold_off_time;

    // Server object values
    time_t   lifetime;
    time_t   default_minimum_period;
    time_t   default_maximum_period;
    time_t   disable_timeout;
    bool     notification_storing_on_disabled;
    char     binding[SERVER_BINDING_SIZE_MAX];

    // ACL values
    uint16_t access[1+LWM2M_MAX_SERVERS];                              /**< ACL array. */
    uint16_t server[1+LWM2M_MAX_SERVERS];                              /**< Short server id to ACL array index. */
    uint16_t owner;                                                    /**< Owner of this ACL entry. Short server id */

    // Local values
    bool     is_bootstrapped;
    bool     is_registered;
    time_t   hold_off_timer;
} server_settings_t;

static server_settings_t     m_server_settings[1+LWM2M_MAX_SERVERS];

#if APP_USE_NVS
/* NVS-related defines */
#define NVS_SECTOR_SIZE    FLASH_ERASE_BLOCK_SIZE    /* Multiple of FLASH_PAGE_SIZE */
#define NVS_SECTOR_COUNT   3                         /* At least 2 sectors */
#define NVS_STORAGE_OFFSET FLASH_AREA_STORAGE_OFFSET /* Start address of the filesystem in flash */

static struct nvs_fs fs = {
    .sector_size  = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset       = NVS_STORAGE_OFFSET,
};
#endif


static uint32_t app_store_bootstrap_server_values(uint16_t instance_id);
static void app_server_update(uint16_t instance_id);
static void app_server_deregister(uint16_t instance_id);
static void app_provision_secret_key(void);


/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF in pairs (1 and 3, 2 and 4)
     * if there is an recoverable error.
     */
    while (true) {
        dk_set_leds_state(DK_LED1_MSK | DK_LED3_MSK, DK_LED2_MSK | DK_LED4_MSK);
        k_sleep(250);
        dk_set_leds_state(DK_LED2_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED3_MSK);
        k_sleep(250);
    }
}


/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF if there is an irrecoverable error. */
    while (true) {
        dk_set_leds_state(DK_ALL_LEDS_MSK, 0x00);
        k_sleep(250);
        dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
        k_sleep(250);
    }
}


#if APP_USE_BUTTONS_AND_LEDS
/**@brief Callback for button events from the DK buttons and LEDs library. */
static void app_button_handler(u32_t buttons, u32_t has_changed)
{
    if (buttons & 0x01) // Button 1 has changed
    {
        if (m_app_state == APP_STATE_IP_INTERFACE_UP)
        {
            if (m_server_settings[0].is_bootstrapped)
            {
                m_app_state = APP_STATE_BOOTSTRAPED;
            }
            else
            {
                m_app_state = APP_STATE_BS_CONNECT;
            }
        }
        else if (m_app_state == APP_STATE_SERVER_REGISTERED)
        {
            printk("app_server_update()\n");
            app_server_update(m_server_instance);
        }
    }
    else if (buttons & 0x02) // Button 2 has changed
    {
        if (m_app_state == APP_STATE_SERVER_REGISTERED)
        {
            m_app_state = APP_STATE_SERVER_DEREGISTER;
        }
    }
}

/**@brief Update LEDs state. */
static void app_leds_update(struct k_work *work)
{
        static bool led_on;
        static u8_t current_led_on_mask;
        u8_t led_on_mask;

        ARG_UNUSED(work);

        /* Set led_on_mask to match current state. */
        led_on_mask = LED_GET_ON(m_app_state);

        if (m_did_bootstrap)
        {
            /* Only turn on LED2 if bootstrap was done. */
            led_on_mask |= LED_ON(DK_LED2_MSK);
        }

        led_on = !led_on;
        if (led_on) {
                led_on_mask |= LED_GET_BLINK(m_app_state);
        } else {
                led_on_mask &= ~LED_GET_BLINK(m_app_state);
        }

        if (led_on_mask != current_led_on_mask) {
                dk_set_leds(led_on_mask);
                current_led_on_mask = led_on_mask;
        }

        k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
}
#endif


/**@brief Initializes buttons and LEDs, using the DK buttons and LEDs library. */
static void app_buttons_leds_init(void)
{
#if APP_USE_BUTTONS_AND_LEDS
    dk_buttons_init(app_button_handler);
    dk_leds_init();
    dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);

    k_delayed_work_init(&leds_update_work, app_leds_update);
    k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
#endif
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
#if 0
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    ARG_UNUSED(wall_clock_value);
#if APP_USE_BUTTONS_AND_LEDS
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


static uint32_t app_resolve_server_uri(char            * server_uri,
                                       struct sockaddr * addr,
                                       bool            * secure)
{
    // Create a string copy to null-terminate hostname within the server_uri.
    char server_uri_val[SECURITY_SERVER_URI_SIZE_MAX];
    strcpy(server_uri_val, server_uri);

    const char *hostname;
    uint16_t port;

    if (strncmp(server_uri_val, "coaps://", 8) == 0) {
        hostname = &server_uri_val[8];
        port = 5684;
        *secure = true;
    } else if (strncmp(server_uri_val, "coap://", 7) == 0) {
        hostname = &server_uri_val[7];
        port = 5683;
        *secure = false;
    } else {
        return EINVAL;
    }

    char *sep = strchr(hostname, ':');
    if (sep) {
        *sep = '\0';
        port = atoi(sep + 1);
    }

    struct addrinfo hints = {
#if (APP_USE_AF_INET6 == 1)
        .ai_family = AF_INET6,
#else // APP_USE_AF_INET6
        .ai_family = AF_INET,
#endif // APP_USE_AF_INET6
        .ai_socktype = SOCK_DGRAM
    };
    struct addrinfo *result;
    int ret_val = getaddrinfo(hostname, NULL, &hints, &result);

    if (ret_val != 0) {
        return errno;
    }

    if (result->ai_family == AF_INET) {
        ((struct sockaddr_in *)addr)->sin_family = result->ai_family;
        ((struct sockaddr_in *)addr)->sin_port = htons(port);
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
    } else {
        ((struct sockaddr_in6 *)addr)->sin6_family = result->ai_family;
        ((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
        memcpy(((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, ((struct sockaddr_in6 *)result->ai_addr)->sin6_addr.s6_addr, 16);
    }

    freeaddrinfo(result);

    return 0;
}


/**@brief Helper function to parse the uri and save the remote to the LWM2M remote database. */
static uint32_t app_lwm2m_parse_uri_and_save_remote(uint16_t          short_server_id,
                                                    char            * server_uri,
                                                    bool            * secure,
                                                    struct sockaddr * p_remote)
{
    uint32_t err_code;

    // Register the short_server_id
    err_code = lwm2m_remote_register(short_server_id);
    if (err_code != 0)
    {
        return err_code;
    }

    // Use DNS to lookup the IP
    printk(" -> doing DNS lookup\n");
    err_code = app_resolve_server_uri(server_uri, p_remote, secure);
    if (err_code != 0)
    {
        printf("app_resolve_server_uri(\"%s\") failed %lu\n", server_uri, err_code);
        return err_code;
    }
    printk(" -> done\n");

    // Save the short_server_id
    err_code = lwm2m_remote_remote_save((struct sockaddr *)p_remote, short_server_id);

    if (err_code != 0)
    {
        return err_code;
    }

    return err_code;
}


/**@brief Helper function to get the access from an instance and a remote. */
static uint32_t app_lwm2m_access_remote_get(uint16_t         * p_access,
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
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_UPDATE)
    {
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_DEREGISTER)
    {
        // We have successfully deregistered, free up the allocated link format string.
        if (mp_link_format_string != NULL)
        {
            // No more attempts, clean up.
            k_free(mp_link_format_string);
            mp_link_format_string = NULL;
        }

        m_app_state = APP_STATE_DISCONNECT;
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
    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

    app_provision_secret_key();

    m_app_state = APP_STATE_BOOTSTRAPED;
    m_server_settings[0].is_bootstrapped = true;
    m_did_bootstrap = true;

    return 0;
}


static uint32_t tlv_security_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t    index = 0;
    uint32_t    err_code = 0;
    lwm2m_tlv_t tlv;

    while (index < p_tlv->length)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_tlv->value, p_tlv->length);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case 0: // HoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_security_hold_off_timer[instance_id]);
                break;
            }
            case 1: // IsBootstrapped
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_security_is_bootstrapped[instance_id]);
                break;
            }
            default:
                break;
        }
    }

    return err_code;
}


static uint32_t tlv_security_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_security_verizon_decode(instance_id, p_tlv);
            break;

        default:
            err_code = ENOENT;
            break;
    }

    return err_code;
}


static uint32_t tlv_server_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    int32_t list_values[2] =
    {
        m_server_is_registered[instance_id],
        m_server_client_hold_off_timer[instance_id]
    };

    lwm2m_list_t list =
    {
        .type        = LWM2M_LIST_TYPE_INT32,
        .p_id        = NULL,
        .val.p_int32 = list_values,
        .len         = 2
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}


static uint32_t tlv_server_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t    index = 0;
    uint32_t    err_code = 0;
    lwm2m_tlv_t tlv;

    while (index < p_tlv->length)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_tlv->value, p_tlv->length);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case 0: // IsRegistered
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_server_is_registered[instance_id]);
                break;
            }
            case 1: // ClientHoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_server_client_hold_off_timer[instance_id]);
                break;
            }
            default:
                break;
        }
    }

    return err_code;
}


static uint32_t tlv_server_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_server_verizon_decode(instance_id, p_tlv);
            break;

        default:
            err_code = ENOENT;
            break;
    }

    return err_code;
}


static uint32_t tlv_conn_mon_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    ARG_UNUSED(instance_id);

    lwm2m_list_t list =
    {
        .type         = LWM2M_LIST_TYPE_STRING,
        .val.p_string = m_apn,
        .len          = 3,
        .max_len      = ARRAY_SIZE(m_apn)
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}


static uint32_t tlv_conn_mon_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t    index = 0;
    uint32_t    err_code = 0;
    lwm2m_tlv_t tlv;

    while (index < p_tlv->length)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_tlv->value, p_tlv->length);

        if (err_code != 0)
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case 0: // Class 2 APN
            {
                // READ only
                err_code = ENOENT;
                break;
            }

            case 1: // Class 3 APN for Internet
            case 2: // Class 6 APN for Enterprise
            case 3: // Class 7 APN for Thingspace
            {
                err_code = lwm2m_bytebuffer_to_string((char *)tlv.value,
                                                      tlv.length,
                                                      &m_apn[tlv.id]);
                break;
            }

            default:
                err_code = ENOENT;
                break;
        }
    }

    return err_code;
}


static uint32_t tlv_conn_mon_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_conn_mon_verizon_decode(instance_id, p_tlv);
            break;

        default:
            err_code = ENOENT;
            break;
    }

    return err_code;
}


uint32_t resource_tlv_callback(lwm2m_instance_t * p_instance, lwm2m_tlv_t * p_tlv)
{
    uint32_t err_code;

    switch (p_instance->object_id)
    {
        case LWM2M_OBJ_SECURITY:
            err_code = tlv_security_resource_decode(p_instance->instance_id, p_tlv);
            break;

        case LWM2M_OBJ_SERVER:
            err_code = tlv_server_resource_decode(p_instance->instance_id, p_tlv);
            break;

        case LWM2M_OBJ_CONN_MON:
            err_code = tlv_conn_mon_resource_decode(p_instance->instance_id, p_tlv);
            break;

        default:
            err_code = ENOENT;
            break;
    }

    return err_code;
}


/**@brief Callback function for LWM2M server instances. */
uint32_t server_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t *   p_request)
{
    APPL_LOG("lwm2m: server_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
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

        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_server_verizon_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
            err_code = lwm2m_tlv_server_encode(buffer,
                                               &buffer_size,
                                               resource_id,
                                               &m_instance_server[instance_id]);

            if (err_code == ENOENT)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                return 0;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                uint32_t added_size = sizeof(buffer) - buffer_size;
                err_code = tlv_server_verizon_encode(instance_id, buffer + buffer_size, &added_size);
                buffer_size += added_size;
            }
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
                                               p_request->payload_len,
                                               resource_tlv_callback);
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
            if (app_store_bootstrap_server_values(instance_id) == 0)
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
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
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
                                               p_request->payload_len,
                                               resource_tlv_callback);
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
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                // TODO: Shutdown and reboot
                NVIC_SystemReset();
                break;
            }

            case LWM2M_DEVICE_FACTORY_RESET:
            {
#if APP_USE_NVS
                for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
                {
                    nvs_delete(&fs, i);
                }
#endif

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
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
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

        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_conn_mon_verizon_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
            err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                &buffer_size,
                                                                resource_id,
                                                                &m_instance_conn_mon);
            if (err_code == ENOENT)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                return 0;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                uint32_t added_size = sizeof(buffer) - buffer_size;
                err_code = tlv_conn_mon_verizon_encode(instance_id, buffer + buffer_size, &added_size);
                buffer_size += added_size;
            }
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
            err_code = lwm2m_tlv_connectivity_monitoring_decode(&m_instance_conn_mon,
                                                                p_request->p_payload,
                                                                p_request->payload_len,
                                                                resource_tlv_callback);
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
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}


static uint32_t app_store_bootstrap_security_values(uint16_t instance_id)
{
    if ((m_instance_security[instance_id].server_uri.len >= SECURITY_SERVER_URI_SIZE_MAX) ||
        (m_instance_security[instance_id].sms_number.len >= SECURITY_SMS_NUMBER_SIZE_MAX))
    {
        // URI or SMS number was to long to be copied.
        return EINVAL;
    }

    m_server_settings[instance_id].is_bootstrap_server  = m_instance_security[instance_id].bootstrap_server;
    m_server_settings[instance_id].sms_security_mode    = m_instance_security[instance_id].sms_security_mode;
    m_server_settings[instance_id].short_server_id      = m_instance_security[instance_id].short_server_id;
    m_server_settings[instance_id].client_hold_off_time = m_instance_security[instance_id].client_hold_off_time;

    // Copy the URI.
    memset(m_server_settings[instance_id].server_uri, 0, SECURITY_SERVER_URI_SIZE_MAX);
    memcpy(m_server_settings[instance_id].server_uri,
           m_instance_security[instance_id].server_uri.p_val,
           m_instance_security[instance_id].server_uri.len);
    m_instance_security[instance_id].server_uri.p_val = m_server_settings[instance_id].server_uri;

    // Copy SMS number.
    memset(m_server_settings[instance_id].sms_number, 0, SECURITY_SMS_NUMBER_SIZE_MAX);
    memcpy(m_server_settings[instance_id].sms_number,
           m_instance_security[instance_id].sms_number.p_val,
           m_instance_security[instance_id].sms_number.len);
    m_instance_security[instance_id].sms_number.p_val = m_server_settings[instance_id].sms_number;

#if APP_USE_NVS
    nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[instance_id]));
#endif

    return 0;
}

static uint32_t app_store_bootstrap_server_values(uint16_t instance_id)
{
    if (m_instance_server[instance_id].binding.len >= SERVER_BINDING_SIZE_MAX)
    {
        // Binding was to long to be copied.
        return EINVAL;
    }

    m_server_settings[instance_id].lifetime = m_instance_server[instance_id].lifetime;
    m_server_settings[instance_id].default_minimum_period = m_instance_server[instance_id].default_minimum_period;
    m_server_settings[instance_id].default_maximum_period = m_instance_server[instance_id].default_maximum_period;
    m_server_settings[instance_id].disable_timeout = m_instance_server[instance_id].disable_timeout;
    m_server_settings[instance_id].notification_storing_on_disabled = m_instance_server[instance_id].notification_storing_on_disabled;

    // Copy Binding.
    memset(m_server_settings[instance_id].binding, 0, SERVER_BINDING_SIZE_MAX);
    memcpy(m_server_settings[instance_id].binding,
           m_instance_server[instance_id].binding.p_val,
           m_instance_server[instance_id].binding.len);

    // Copy ACL.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *) &m_instance_server[instance_id].proto;
    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++)
    {
        if (p_instance->acl.server[i])
        {
            m_server_settings[instance_id].access[i] = p_instance->acl.access[i];
            m_server_settings[instance_id].server[i] = p_instance->acl.server[i];
        }
    }
    m_server_settings[instance_id].owner = p_instance->acl.owner;

#if APP_USE_NVS
    nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[instance_id]));
#endif

    return 0;
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
        (void)lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                      p_request->p_payload,
                                      p_request->payload_len,
                                      resource_tlv_callback);

        m_instance_server[instance_id].proto.instance_id = instance_id;
        m_instance_server[instance_id].proto.object_id   = p_object->object_id;
        m_instance_server[instance_id].proto.callback    = server_instance_callback;

        if (app_store_bootstrap_server_values(instance_id) == 0)
        {
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
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

#if 0
uint32_t security_resource_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t   * p_request)
{
    uint16_t instance_id = p_instance->instance_id;
    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        // Write to p_request->p_payload, update p_request->payload_len
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        // uint32_t mask = 0;
        // err_code = coap_message_ct_mask_get(p_request, &mask);

        // Read from p_request->payload, from offset XXX
    }
    else
    {
        err_code = ENOENT;
    }

    return err_code;
}
#endif


/**@brief Callback function for LWM2M security instances. */
uint32_t security_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    APPL_LOG("lwm2m: security_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
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

        err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                             p_request->p_payload,
                                             p_request->payload_len,
                                             resource_tlv_callback);
        APP_ERROR_CHECK(err_code);

        if (app_store_bootstrap_security_values(instance_id) == 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else
        {
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
        uint32_t err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                                      p_request->p_payload,
                                                      p_request->payload_len,
                                                      resource_tlv_callback);
        APP_ERROR_CHECK(err_code);

        // Copy public key as string
        size_t public_key_len = m_instance_security[instance_id].public_key.len;
        m_public_key[instance_id] = k_malloc(public_key_len + 1);
        memcpy(m_public_key[instance_id], m_instance_security[instance_id].public_key.p_val, public_key_len);
        m_public_key[instance_id][public_key_len] = 0;

        // Convert secret key from binary to string
        size_t secret_key_len = m_instance_security[instance_id].secret_key.len * 2;
        m_secret_key[instance_id] = k_malloc(secret_key_len + 1);
        for (int i = 0; i < m_instance_security[instance_id].secret_key.len; i++) {
            sprintf(&m_secret_key[instance_id][i*2], "%02x", m_instance_security[instance_id].secret_key.p_val[i]);
        }
        m_secret_key[instance_id][secret_key_len] = 0;

        printk(" -> secret key %d: %s\n", instance_id, m_secret_key[instance_id]);

        APPL_LOG("lwm2m: decoded security.");

        m_instance_security[instance_id].proto.instance_id = instance_id;
        m_instance_security[instance_id].proto.object_id   = p_object->object_id;
        m_instance_security[instance_id].proto.callback    = security_instance_callback;

        if (app_store_bootstrap_security_values(instance_id) == 0)
        {
            // No ACL object for security objects.
            ((lwm2m_instance_t *)&m_instance_security[instance_id])->acl.id = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;

            // Cast the instance to its prototype and add it to the CoAP handler to become a
            // public instance. We can only have one so we delete the first if any.
            (void)lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)&m_instance_security[instance_id]);

            (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_security[instance_id]);

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


/**@brief Create factory bootstrapped server objects.
 *        Depends on carrier, this is Verizon / MotiveBridge.
 */
static void app_factory_bootstrap_server_object(uint16_t instance_id)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    switch (instance_id)
    {
        case 0: // Bootstrap server
        {
            memset(&m_server_settings[0], 0, sizeof(m_server_settings[0]));
            m_server_settings[0].short_server_id = 100;
            strcpy(m_server_settings[0].server_uri, BOOTSTRAP_URI);
            m_server_settings[0].is_bootstrap_server = true;
            m_server_settings[0].access[0] = rwde_access;
            m_server_settings[0].server[0] = 102;
            m_server_settings[0].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
            break;
        }

        case 1: // DM server
        {
            memset(&m_server_settings[1], 0, sizeof(m_server_settings[1]));
            break;
        }

        case 2: // Diagnostics server
        {
            memset(&m_server_settings[2], 0, sizeof(m_server_settings[2]));
            m_server_settings[2].short_server_id = 101;
            strcpy(m_server_settings[2].server_uri, "");
            m_server_settings[2].lifetime = 86400;
            m_server_settings[2].default_minimum_period = 300;
            m_server_settings[2].default_maximum_period = 6000;
            m_server_settings[2].notification_storing_on_disabled = 1;
            strcpy(m_server_settings[2].binding, "UQS");
            m_server_settings[2].access[0] = rwde_access;
            m_server_settings[2].server[0] = 102;
            m_server_settings[2].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
            break;
        }

        case 3: // Repository server
        {
            memset(&m_server_settings[3], 0, sizeof(m_server_settings[3]));
            break;
        }

        default:
            break;
    }
}


static void app_read_flash_storage(void)
{
#if APP_USE_NVS
    int rc;

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        rc = nvs_read(&fs, i, &m_server_settings[i], sizeof(m_server_settings[i]));
        if (rc <= 0)
        {
            // Not found, create new factory bootstrapped object.
            app_factory_bootstrap_server_object(i);

            if (m_server_settings[i].short_server_id)
            {
                // Write settings for initialized server objects.
                nvs_write(&fs, i, &m_server_settings[i], sizeof(m_server_settings[i]));
            }
        }
    }
#else
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        app_factory_bootstrap_server_object(i);
    }

    // Workaround for not storing is_bootstrapped:
    // - Switch 1 will determine if doing bootstrap
    // - Switch 2 will determine if connecting to DM or Repository server

    u32_t button_state = 0;
    dk_read_buttons(&button_state, NULL);

    if (button_state & 0x04) // Switch 1 in left position
    {
        m_server_settings[0].is_bootstrapped = false;
    }
    else
    {
        m_server_settings[0].is_bootstrapped = true;
    }

    if (button_state & 0x08) // Switch 2 in left position
    {
        m_server_instance = 1; // Connect to DM server
    }
    else
    {
        m_server_instance = 3; // Connect to Repository server
    }

    // Bootstrap values (will be fetched from NVS after bootstrap)
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);


    // DM server
    m_server_settings[1].short_server_id = 102;
    strcpy(m_server_settings[1].server_uri, "coaps://ddocdp.do.motive.com:5684");
    m_server_settings[1].lifetime = 2592000;
    m_server_settings[1].default_minimum_period = 1;
    m_server_settings[1].default_maximum_period = 60;
    m_server_settings[1].disable_timeout = 86400;
    m_server_settings[1].notification_storing_on_disabled = 1;
    strcpy(m_server_settings[1].binding, "UQS");
    m_server_settings[1].access[0] = rwde_access;
    m_server_settings[1].server[0] = 101;
    m_server_settings[1].access[1] = rwde_access;
    m_server_settings[1].server[1] = 102;
    m_server_settings[1].access[2] = rwde_access;
    m_server_settings[1].server[2] = 1000;

    if (m_server_settings[0].is_bootstrapped)
    {
        m_server_settings[1].owner = 102;
    }
    else
    {
        m_server_settings[1].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
    }

    // Repository server
    m_server_settings[3].short_server_id = 1000;
    strcpy(m_server_settings[3].server_uri, "coaps://xvzwmpctii.xdev.motive.com:5684");
    m_server_settings[3].lifetime = 86400;
    m_server_settings[3].default_minimum_period = 1;
    m_server_settings[3].default_maximum_period = 6000;
    m_server_settings[3].disable_timeout = 86400;
    m_server_settings[3].notification_storing_on_disabled = 1;
    strcpy(m_server_settings[3].binding, "UQ");
    m_server_settings[3].access[0] = rwde_access;
    m_server_settings[3].server[0] = 101;
    m_server_settings[3].access[1] = rwde_access;
    m_server_settings[3].server[1] = 102;
    m_server_settings[3].access[2] = rwde_access;
    m_server_settings[3].server[2] = 1000;
    m_server_settings[3].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
#endif
}


static void app_lwm2m_create_objects(void)
{
    app_read_flash_storage();

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        m_instance_server[i].short_server_id = m_server_settings[i].short_server_id;
        m_instance_server[i].lifetime = m_server_settings[i].lifetime;
        m_instance_server[i].default_minimum_period = m_server_settings[i].default_minimum_period;
        m_instance_server[i].default_maximum_period = m_server_settings[i].default_maximum_period;
        m_instance_server[i].disable_timeout = m_server_settings[i].disable_timeout;
        m_instance_server[i].notification_storing_on_disabled = m_server_settings[i].notification_storing_on_disabled;

        (void)lwm2m_bytebuffer_to_string(m_server_settings[i].binding,
                                         strlen(m_server_settings[i].binding),
                                         &m_instance_server[i].binding);

        m_instance_server[i].proto.callback = server_instance_callback;

        m_server_is_registered[i] = 1;
        m_server_client_hold_off_timer[i] = 30;

#if APP_ACL_DM_SERVER_HACK
    }

    // FIXME: Init ACL for DM server[1] first to get ACL /2/0 which is according to Verizon spec
    uint32_t acl_init_order[] = { 1, 0, 2, 3 };
    for (uint32_t k = 0; k < ARRAY_SIZE(acl_init_order); k++)
    {
        uint32_t i = acl_init_order[k];
#endif

        // Initialize ACL on the instance.
        (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_server[i],
                                        m_server_settings[i].owner);

        // Set default access to LWM2M_PERMISSION_READ.
        (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[i],
                                        LWM2M_PERMISSION_READ,
                                        LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

        for (uint32_t j = 0; j < ARRAY_SIZE(m_server_settings[i].server); j++)
        {
            if (m_server_settings[i].server[j] != 0)
            {
                // Set server access.
                (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_server[i],
                                                m_server_settings[i].access[j],
                                                m_server_settings[i].server[j]);
            }
        }

        (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_server[i]);
    }

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
    (void)lwm2m_bytebuffer_to_string("UQS", 3, &m_instance_device.supported_bindings);
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

    char * class2_apn = "VZWADMIN";
    (void)lwm2m_bytebuffer_to_string(class2_apn, strlen(class2_apn), &m_apn[0]);
    char * class3_apn = "VZWINTERNET";
    (void)lwm2m_bytebuffer_to_string(class3_apn, strlen(class3_apn), &m_apn[1]);
    char * class6_apn = "VZWCLASS6";
    (void)lwm2m_bytebuffer_to_string(class6_apn, strlen(class6_apn), &m_apn[2]);
    char * class7_apn = "VZWIOTTS";
    (void)lwm2m_bytebuffer_to_string(class7_apn, strlen(class7_apn), &m_apn[3]);

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
static void app_lwm2m_setup(void)
{
    (void)lwm2m_init(k_malloc, k_free);
    (void)lwm2m_remote_init();
    (void)lwm2m_acl_init();

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
    bool secure;

    // Save the remote address of the bootstrap server.
    (void)app_lwm2m_parse_uri_and_save_remote(LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID,
                                              (char *)&m_server_settings[0].server_uri,
                                              &secure,
                                              mp_bs_remote_server);

    if (secure == true)
    {
        APPL_LOG("SECURE session (bootstrap)");

#if (APP_USE_AF_INET6 == 1)
            const struct sockaddr_in6 client_addr =
            {
                .sin6_port   = htons(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin6_family = AF_INET6,
            };
#else // APP_USE_AF_INET6
            const struct sockaddr_in client_addr =
            {
                .sin_port   = htons(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin_family = AF_INET,
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
    bool secure;

    // Initialize server configuration structure.
    memset(&m_server_conf, 0, sizeof(lwm2m_server_config_t));
    m_server_conf.lifetime = 1000;

    // Set the short server id of the server in the config.
    m_server_conf.short_server_id = m_server_settings[m_server_instance].short_server_id;

    // Save the remote address of the server.
    (void)app_lwm2m_parse_uri_and_save_remote(m_instance_server[m_server_instance].short_server_id,
                                              (char *)&m_server_settings[m_server_instance].server_uri,
                                              &secure,
                                              mp_remote_server);

    if (secure == true)
    {
        APPL_LOG("SECURE session (register)");
#if (APP_USE_AF_INET6 == 1)
        const struct sockaddr_in6 client_addr =
        {
            .sin6_port   = htons(LWM2M_LOCAL_CLIENT_PORT),
            .sin6_family = AF_INET6,
        };
#else // APP_USE_AF_INET6
        const struct sockaddr_in client_addr =
        {
            .sin_port   = htons(LWM2M_LOCAL_CLIENT_PORT),
            .sin_family = AF_INET,
        };
#endif // APP_USE_AF_INET6

        #define SEC_TAG_COUNT 1

        struct sockaddr * p_localaddr = (struct sockaddr *)&client_addr;

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = { APP_SEC_TAG_OFFSET + m_server_instance };

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


static void app_server_deregister(uint16_t instance_id)
{
    uint32_t err_code;

    // TODO: check instance_id
    ARG_UNUSED(instance_id);

    err_code = lwm2m_deregister((struct sockaddr *)mp_remote_server,
                                mp_lwm2m_transport);
    APP_ERROR_CHECK(err_code);

    m_app_state = APP_STATE_SERVER_DEREGISTERING;
}


static void app_disconnect(void)
{
    uint32_t err_code;

    // Destroy the secure session if any.
    if (mp_lwm2m_bs_transport)
    {
        err_code = coap_security_destroy(mp_lwm2m_bs_transport);
        APP_ERROR_CHECK(err_code);

        mp_lwm2m_bs_transport = NULL;
    }

    if (mp_lwm2m_transport)
    {
        err_code = coap_security_destroy(mp_lwm2m_transport);
        APP_ERROR_CHECK(err_code);

        mp_lwm2m_transport = NULL;
    }

    m_app_state = APP_STATE_IP_INTERFACE_UP;
}


static void app_lwm2m_process(void)
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
            printk("app_bootstrap()\n");
            app_bootstrap();
            break;
        }
        case APP_STATE_BOOTSTRAPED:
        {
            printk("app_server_connect(\"%s server\")\n", (m_server_instance == 1) ? "DM" : "Repository");
            app_server_connect();
            break;
        }
        case APP_STATE_SERVER_CONNECTED:
        {
            printk("app_server_register()\n");
            app_server_register();
            break;
        }
        case APP_STATE_SERVER_DEREGISTER:
        {
            printk("app_server_deregister()\n");
            app_server_deregister(m_server_instance);
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

static void app_coap_init(void)
{
    uint32_t err_code;

#if (APP_USE_AF_INET6 == 1)
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


static void app_provision_psk(int at_socket_fd, int sec_tag, char * identity, char * psk)
{
    int bytes_written;
    int bytes_read;

    #define WRITE_OPCODE  0
    #define IDENTITY_CODE 4
    #define PSK_CODE      3

    memset(m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer, APP_MAX_AT_WRITE_LENGTH, "AT%%CMNG=%d,%d,%d,\"%s\"",
            WRITE_OPCODE, sec_tag, IDENTITY_CODE, identity);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    memset(m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    snprintf(m_at_write_buffer, APP_MAX_AT_WRITE_LENGTH, "AT%%CMNG=%d,%d,%d,\"%s\"",
             WRITE_OPCODE, sec_tag, PSK_CODE, psk);

    bytes_written = send(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer), 0);
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);
}


static void app_provision_secret_key(void)
{
    int at_socket_fd  = -1;
    int bytes_written;
    int bytes_read;

    printk("app_provision_secret_key()\n");
    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    APP_ERROR_CHECK_BOOL(at_socket_fd >= 0);

    // Enter flight mode
    bytes_written = send(at_socket_fd, "AT+CFUN=4", 9, 0);
    APP_ERROR_CHECK_BOOL(bytes_written == 9);

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    printk(" -> flight mode ok\n");
    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        if (m_public_key[i] && m_secret_key[i])
        {
            // FIXME: use correct sec_tag
            app_provision_psk(at_socket_fd, APP_SEC_TAG_OFFSET+i, m_public_key[i], m_secret_key[i]);

            k_free(m_public_key[i]);
            m_public_key[i] = NULL;
            k_free(m_secret_key[i]);
            m_secret_key[i] = NULL;
        }
    }

    printk(" -> provision secret key ok\n");
    // Enter normal mode
    bytes_written = send(at_socket_fd, "AT+CFUN=1", 9, 0);
    APP_ERROR_CHECK_BOOL(bytes_written == 9);

    bytes_read = recv(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH, 0);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    printk(" -> done\n");
    ARG_UNUSED(close(at_socket_fd));

    // FIXME: figure out why this is needed.
    k_sleep(5000);
}


/**@brief Function to provision credentials used for secure transport by the CoAP client. */
static void app_provision(void)
{
    int at_socket_fd  = -1;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    APP_ERROR_CHECK_BOOL(at_socket_fd >= 0);

    app_provision_psk(at_socket_fd, APP_BOOTSTRAP_SEC_TAG, APP_BOOTSTRAP_SEC_IDENTITY, APP_BOOTSTRAP_SEC_PSK);
#if 0
    app_provision_psk(at_socket_fd, APP_DM_SERVER_SEC_TAG, APP_DM_SERVER_SEC_IDENTITY, APP_DM_SERVER_SEC_PSK);
    app_provision_psk(at_socket_fd, APP_RS_SERVER_SEC_TAG, APP_RS_SERVER_SEC_IDENTITY, APP_RS_SERVER_SEC_PSK);
#endif

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
}


/**@brief Initialize Non-volatile Storage. */
static void app_flash_init(void)
{
#if APP_USE_NVS
    int rc = nvs_init(&fs, DT_FLASH_DEV_NAME);
    if (rc) {
        printk("Flash init failed: %d\n", rc);
    }
#endif
}


/**@brief Function for application main entry.
 */
int main(void)
{
    printk("Application started\n");

    // Initialize LEDs and Buttons.
    app_buttons_leds_init();

    app_flash_init();

    // Provision credentials used for the bootstrap server.
    app_provision();

    // Initialize CoAP.
    app_coap_init();

    // Setup LWM2M endpoints.
    app_lwm2m_setup();

    // Create LwM2M factory bootstraped objects.
    app_lwm2m_create_objects();

    // Establish LTE link.
    app_modem_configure();
    //lte_lc_init_and_connect();

    // Initialize Timers.
    //timers_init();
    //iot_timer_init();

    if (m_server_settings[0].is_bootstrapped)
    {
        m_app_state = APP_STATE_BOOTSTRAPED;
    }
    else
    {
        m_app_state = APP_STATE_BS_CONNECT;
    }

    // Enter main loop
    for (;;)
    {
        app_lwm2m_process();
    }
}

/**
 * @}
 */
