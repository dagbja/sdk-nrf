/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LOG_MODULE_NAME lwm2m_client

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <net/socket.h>
#include <nvs/nvs.h>
#include <lte_lc.h>
#include <nrf_inbuilt_key.h>
#include <nrf.h>

#if CONFIG_SHELL
#include <shell/shell.h>
#endif

#if CONFIG_DK_LIBRARY
#include <dk_buttons_and_leds.h>
#endif

#if CONFIG_AT_HOST_LIBRARY
#include <at_host.h>
#endif

#include <net/coap_api.h>
#include <net/coap_option.h>
#include <net/coap_message.h>
#include <net/coap_observe_api.h>
#include <lwm2m_api.h>
#include <lwm2m_remote.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>

/* Hardcoded IMEI for now, will be fetched from modem using AT+CGSN=1 */
#define IMEI            "004402990020434"

/* Hardcoded MSISDN fro now, will be fetched from modem using AT+CNUM */
#define MSISDN          "0123456789"

#define APP_MOTIVE_FIX_UPDATE_TRIGGER   1 // To adjust for MotiveBridge posting /1/0/8 instead of /1/1/8
#define APP_MOTIVE_NO_REBOOT            1 // To pass MotiveBridge test 5.10 "Persistency Throughout Device Reboot"
#define APP_MOTIVE_FAKE_POWER_SOURCES   1 // To pass MotiveBridge power source tests (4.10, 4.11 and 4.12)

#define APP_ACL_DM_SERVER_HACK          1
#define APP_USE_CONTABO                 0

#define APP_LEDS_UPDATE_INTERVAL        500                                                   /**< Interval in milliseconds between each time status LEDs are updated. */

#define COAP_LOCAL_LISTENER_PORT              5683                                            /**< Local port to listen on any traffic, client or server. Not bound to any specific LWM2M functionality.*/
#define LWM2M_LOCAL_LISTENER_PORT             9997                                            /**< Local port to listen on any traffic. Bound to specific LWM2M functionality. */
#if APP_USE_CONTABO
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     5784                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5784                                            /**< Remote port of the LWM2M bootstrap server. */
#else
#define LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT     9998                                            /**< Local port to connect to the LWM2M bootstrap server. */
#define LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT    5684                                            /**< Remote port of the LWM2M bootstrap server. */
#endif
#define LWM2M_LOCAL_CLIENT_PORT_OFFSET        9999                                            /**< Local port to connect to the LWM2M server. */
#define LWM2M_SERVER_REMORT_PORT              5684                                            /**< Remote port of the LWM2M server. */

#if APP_USE_CONTABO
#define BOOTSTRAP_URI                   "coaps://vmi36865.contabo.host:5784"                  /**< Server URI to the bootstrap server when using security (DTLS). */
#else
#define BOOTSTRAP_URI                   "coaps://ddocdpboot.do.motive.com:5684"               /**< Server URI to the bootstrap server when using security (DTLS). */
#endif

#define SECURITY_SERVER_URI_SIZE_MAX    64                                                    /**< Max size of server URIs. */
#define SECURITY_SMS_NUMBER_SIZE_MAX    20                                                    /**< Max size of server SMS number. */
#define SERVER_BINDING_SIZE_MAX         4                                                     /**< Max size of server binding. */

#define APP_SEC_TAG_OFFSET              25

#define APP_BOOTSTRAP_SEC_TAG           APP_SEC_TAG_OFFSET                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#if APP_USE_CONTABO
#define APP_BOOTSTRAP_SEC_PSK           "676c656e6e73736563726574"                            /**< Pre-shared key used for bootstrap server in hex format. */
#else
#define APP_BOOTSTRAP_SEC_PSK           "d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747" /**< Pre-shared key used for bootstrap server in hex format. */
#endif

#define APP_MAX_AT_READ_LENGTH          100
#define APP_MAX_AT_WRITE_LENGTH         256

#if CONFIG_LOG
#define APPL_LOG  LOG_DBG
#else
#define APPL_LOG
#endif

#if CONFIG_DK_LIBRARY
#define APP_ERROR_CHECK(error_code) \
    do { \
        if (error_code != 0) { \
            APPL_LOG("Error: %lu", error_code); \
            k_delayed_work_cancel(&leds_update_work); \
            /* Blinking all LEDs ON/OFF in pairs (1 and 2, 3 and 4) if there is an error. */ \
            while (true) { \
                dk_set_leds_state(DK_LED1_MSK | DK_LED2_MSK, DK_LED3_MSK | DK_LED4_MSK); \
                k_sleep(250); \
                dk_set_leds_state(DK_LED3_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED2_MSK); \
                k_sleep(250); \
            } \
        } \
    } while (0)

#define APP_ERROR_CHECK_BOOL(boolean_value) \
    do { \
        const uint32_t local_value = (boolean_value); \
        if (!local_value) { \
            APPL_LOG("BOOL check failure"); \
            k_delayed_work_cancel(&leds_update_work); \
            /* Blinking all LEDs ON/OFF in pairs (1 and 2, 3 and 4) if there is an error. */ \
            while (true) { \
                dk_set_leds_state(DK_LED1_MSK | DK_LED2_MSK, DK_LED3_MSK | DK_LED4_MSK); \
                k_sleep(250); \
                dk_set_leds_state(DK_LED3_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED2_MSK); \
                k_sleep(250); \
            } \
        } \
    } while (0)
#else
#define APP_ERROR_CHECK(error_code) \
    do { \
        if (error_code != 0) { \
            APPL_LOG("Error: %lu", error_code); \
            while (1) \
                ; \
        } \
    } while (0)

#define APP_ERROR_CHECK_BOOL(boolean_value) \
    do { \
        const uint32_t local_value = (boolean_value); \
        if (!local_value) { \
            APPL_LOG("BOOL check failure"); \
            while (1) \
                ; \
        } \
    } while (0)
#endif

typedef enum
{
    APP_STATE_IDLE,
    APP_STATE_IP_INTERFACE_UP,
    APP_STATE_BS_CONNECT,
    APP_STATE_BS_CONNECT_WAIT,
    APP_STATE_BS_CONNECTED,
    APP_STATE_BOOTSTRAP_REQUESTED,
    APP_STATE_BOOTSTRAP_WAIT,
    APP_STATE_BOOTSTRAPPING,
    APP_STATE_BOOTSTRAPPED,
    APP_STATE_SERVER_CONNECT,
    APP_STATE_SERVER_CONNECT_WAIT,
    APP_STATE_SERVER_CONNECTED,
    APP_STATE_SERVER_REGISTER_WAIT,
    APP_STATE_SERVER_REGISTERED,
    APP_STATE_SERVER_DEREGISTER,
    APP_STATE_SERVER_DEREGISTERING,
    APP_STATE_DISCONNECT
} app_state_t;

static lwm2m_server_config_t               m_server_conf[1+LWM2M_MAX_SERVERS];                /**< Server configuration structure. */
static lwm2m_client_identity_t             m_client_id;                                       /**< Client ID structure to hold the client's UUID. */

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

static lwm2m_string_t                      m_apn[4];                                          /**< Verizon specific APN names. */

static coap_transport_handle_t *           mp_coap_transport;                                 /**< CoAP transport handle for the non bootstrap server. */
static coap_transport_handle_t *           mp_lwm2m_bs_transport;                             /**< CoAP transport handle for the secure bootstrap server. Obtained on @coap_security_setup. */
static coap_transport_handle_t *           mp_lwm2m_transport[1+LWM2M_MAX_SERVERS];           /**< CoAP transport handle for the secure server. Obtained on @coap_security_setup. */

static volatile app_state_t m_app_state = APP_STATE_IDLE;                                     /**< Application state. Should be one of @ref app_state_t. */
static volatile uint16_t    m_server_instance;                                                /**< Server instance handled. */
static volatile bool        m_did_bootstrap;
static volatile uint16_t    m_update_server;

static char *               m_public_key[1+LWM2M_MAX_SERVERS];
static char *               m_secret_key[1+LWM2M_MAX_SERVERS];

// TODO: different retries for different vendors?
#if APP_USE_CONTABO
static s32_t app_retry_delay[] = { 2, 4, 6, 8, 24*60 };
#else
static s32_t app_retry_delay[] = { 2*60, 4*60, 6*60, 8*60, 24*60*60 };
#endif

/* Structures for delayed work */
static struct k_delayed_work state_update_work;
#if CONFIG_DK_LIBRARY
static struct k_delayed_work leds_update_work;
#endif

static struct k_delayed_work connection_update_work[1+LWM2M_MAX_SERVERS];


/* Resolved server addresses */
#if APP_USE_CONTABO
static sa_family_t m_family_type[1+LWM2M_MAX_SERVERS] = { AF_INET, AF_INET, 0, AF_INET };  /**< Current IP versions, start using IPv6. */
#else
static sa_family_t m_family_type[1+LWM2M_MAX_SERVERS] = { AF_INET6, AF_INET6, 0, AF_INET6 };  /**< Current IP versions, start using IPv6. */
#endif
static struct sockaddr m_bs_remote_server;                                                    /**< Remote bootstrap server address to connect to. */

static struct sockaddr m_remote_server[1+LWM2M_MAX_SERVERS];                                  /**< Remote secure server address to connect to. */
static volatile uint32_t tick_count = 0;


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
    union {
        uint32_t bootstrapped;
        uint32_t registered;
    } is;
    uint32_t hold_off_timer;      /**< The number of seconds to wait before attempting bootstrap or registration. */
    uint32_t retry_count;         /**< The number of unsuccessful registration retries to reach the server. */
} server_settings_t;

static server_settings_t     m_server_settings[1+LWM2M_MAX_SERVERS];

/**@brief Configurable device values. */
typedef struct {
    char imei[16];
    char msisdn[16];
    char manufacturer[64];
    char model_number[16];
    char serial_number[16];
    char modem_logging[65];
} device_settings_t;

static device_settings_t m_device_settings;
#define DEVICE_FLASH_ID 10

#if CONFIG_FLASH
/* NVS-related defines */
#define NVS_SECTOR_SIZE    FLASH_ERASE_BLOCK_SIZE    /* Multiple of FLASH_PAGE_SIZE */
#define NVS_SECTOR_COUNT   2                         /* At least 2 sectors */
#define NVS_STORAGE_OFFSET FLASH_AREA_STORAGE_OFFSET /* Start address of the filesystem in flash */

static struct nvs_fs fs = {
    .sector_size  = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset       = NVS_STORAGE_OFFSET,
};
#endif


static uint32_t app_store_bootstrap_server_acl(uint16_t instance_id);
static uint32_t app_store_bootstrap_server_values(uint16_t instance_id);
static void app_server_update(uint16_t instance_id);
static void app_server_deregister(uint16_t instance_id);
static void app_provision_psk(int sec_tag, char * identity, char * psk);
static void app_provision_secret_keys(void);
static void app_disconnect(void);
static void app_factory_reset(void);
static void app_wait_state_update(struct k_work *work);
static const char * app_uri_get(char * server_uri, uint16_t * p_port, bool * p_secure);

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

#if CONFIG_DK_LIBRARY
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
#else
    while (true);
#endif
}


/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t error)
{
    ARG_UNUSED(error);

#if CONFIG_DK_LIBRARY
    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF if there is an irrecoverable error. */
    while (true) {
        dk_set_leds_state(DK_ALL_LEDS_MSK, 0x00);
        k_sleep(250);
        dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
        k_sleep(250);
    }
#else
    while (true);
#endif
}


#if CONFIG_DK_LIBRARY
/**@brief Callback for button events from the DK buttons and LEDs library. */
static void app_button_handler(u32_t buttons, u32_t has_changed)
{
    if (buttons & 0x01) // Button 1 has changed
    {
        if (m_app_state == APP_STATE_IP_INTERFACE_UP)
        {
            if (m_server_settings[0].is.bootstrapped)
            {
                m_app_state = APP_STATE_SERVER_CONNECT;
            }
            else
            {
                m_app_state = APP_STATE_BS_CONNECT;
            }
        }
        else if (m_app_state == APP_STATE_SERVER_REGISTERED)
        {
            m_update_server = 1;
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


static void app_leds_get_state(u8_t *on, u8_t *blink)
{
    *on = 0;
    *blink = 0;

    switch (m_app_state)
    {
        case APP_STATE_IDLE:
            *blink = DK_LED1_MSK;
            break;

        case APP_STATE_IP_INTERFACE_UP:
            *on = DK_LED1_MSK;
            break;

        case APP_STATE_BS_CONNECT:
            *blink = (DK_LED1_MSK | DK_LED2_MSK);
            break;

        case APP_STATE_BS_CONNECT_WAIT:
            *blink = (DK_LED2_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_BS_CONNECTED:
        case APP_STATE_BOOTSTRAP_REQUESTED:
            *on = DK_LED1_MSK;
            *blink = DK_LED2_MSK;
            break;

        case APP_STATE_BOOTSTRAP_WAIT:
            *on = DK_LED1_MSK;
            *blink = (DK_LED2_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_BOOTSTRAPPING:
            *on = (DK_LED1_MSK | DK_LED2_MSK);
            *blink = DK_LED4_MSK;
            break;

        case APP_STATE_BOOTSTRAPPED:
            *on = (DK_LED1_MSK | DK_LED2_MSK);
            break;

        case APP_STATE_SERVER_CONNECT:
            *blink = (DK_LED1_MSK | DK_LED3_MSK);
            break;

        case APP_STATE_SERVER_CONNECT_WAIT:
            *blink = (DK_LED3_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_SERVER_CONNECTED:
            *on = DK_LED1_MSK;
            *blink = DK_LED3_MSK;
            break;

        case APP_STATE_SERVER_REGISTER_WAIT:
            *on = DK_LED1_MSK;
            *blink = (DK_LED3_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_SERVER_REGISTERED:
            *on = (DK_LED1_MSK | DK_LED3_MSK);
            break;

        case APP_STATE_SERVER_DEREGISTER:
        case APP_STATE_SERVER_DEREGISTERING:
        case APP_STATE_DISCONNECT:
            *on = DK_LED3_MSK;
            *blink = DK_LED1_MSK;
            break;
    }
}


/**@brief Update LEDs state. */
static void app_leds_update(struct k_work *work)
{
        static bool led_on;
        static u8_t current_led_on_mask;
        u8_t led_on_mask, led_blink_mask;

        ARG_UNUSED(work);

        /* Set led_on_mask to match current state. */
        app_leds_get_state(&led_on_mask, &led_blink_mask);

        if (m_did_bootstrap)
        {
            /* Only turn on LED2 if bootstrap was done. */
            led_on_mask |= DK_LED2_MSK;
        }

        led_on = !led_on;
        if (led_on) {
                led_on_mask |= led_blink_mask;
                if (led_blink_mask == 0) {
                    // Only blink LED4 if no other led is blinking
                    led_on_mask |= DK_LED4_MSK;
                }
        } else {
                led_on_mask &= ~led_blink_mask;
                led_on_mask &= ~DK_LED4_MSK;
        }

        if (led_on_mask != current_led_on_mask) {
                dk_set_leds(led_on_mask);
                current_led_on_mask = led_on_mask;
        }

        k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
}


/**@brief Initializes buttons and LEDs, using the DK buttons and LEDs library. */
static void app_buttons_leds_init(void)
{
    dk_buttons_init(app_button_handler);
    dk_leds_init();
    dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);

    k_delayed_work_init(&leds_update_work, app_leds_update);
    k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
}
#endif


static char * app_client_imei_msisdn()
{
    static char client_id[128];
    extern char imei[];
    extern char msisdn[];

    char * p_imei = imei;
    char * p_msisdn = msisdn;

    if (m_device_settings.imei[0]) {
        p_imei = m_device_settings.imei;
    }

    if (m_device_settings.msisdn[0]) {
        p_msisdn = m_device_settings.msisdn;
    }

    snprintf(client_id, 128, "urn:imei-msisdn:%s-%s", p_imei, p_msisdn);

    return client_id;
}


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


static void app_init_sockaddr_in(struct sockaddr *addr, sa_family_t ai_family, u16_t port)
{
    memset(addr, 0, sizeof(struct sockaddr));

    if (ai_family == AF_INET)
    {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;

        addr_in->sin_family = ai_family;
        addr_in->sin_port = htons(port);
    }
    else
    {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;

        addr_in6->sin6_family = ai_family;
        addr_in6->sin6_port = htons(port);
    }
}

static const char * app_uri_get(char * server_uri, uint16_t * p_port, bool * p_secure) {
    const char *hostname;

    if (strncmp(server_uri, "coaps://", 8) == 0) {
        hostname = &server_uri[8];
        *p_port = 5684;
        *p_secure = true;
    } else if (strncmp(server_uri, "coap://", 7) == 0) {
        hostname = &server_uri[7];
        *p_port = 5683;
        *p_secure = false;
    } else {
        APPL_LOG("Invalid server URI: %s", server_uri);
        return NULL;
    }

    char *sep = strchr(hostname, ':');
    if (sep) {
        *sep = '\0';
        *p_port = atoi(sep + 1);
    }

    return hostname;
}

static uint32_t app_resolve_server_uri(char            * server_uri,
                                       struct sockaddr * addr,
                                       bool            * secure,
                                       uint16_t          instance_id)
{
    // Create a string copy to null-terminate hostname within the server_uri.
    char server_uri_val[SECURITY_SERVER_URI_SIZE_MAX];
    strcpy(server_uri_val, server_uri);

    uint16_t port;
    const char *hostname = app_uri_get(server_uri_val, &port, secure);

    if (hostname == NULL) {
        return EINVAL;
    }

    APPL_LOG("Doing DNS lookup using %s", (m_family_type[instance_id] == AF_INET6) ? "IPv6" : "IPv4");
    struct addrinfo hints = {
        .ai_family = m_family_type[instance_id],
        .ai_socktype = SOCK_DGRAM
    };
    struct addrinfo *result;

    int ret_val = getaddrinfo(hostname, NULL, &hints, &result);

    if (ret_val != 0) {
        APPL_LOG("Failed to lookup \"%s\": %d (%d)", hostname, ret_val, errno);
        return errno;
    }

    app_init_sockaddr_in(addr, result->ai_family, port);

    if (result->ai_family == AF_INET) {
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
        char verbose_ipv4_buffer[27];
        snprintf(verbose_ipv4_buffer,
                 sizeof(verbose_ipv4_buffer),
                 "DNS IPV4 %u.%u.%u.%u",
                 ((uint8_t *)&(((struct sockaddr_in *)addr)->sin_addr.s_addr))[0],
                 ((uint8_t *)&(((struct sockaddr_in *)addr)->sin_addr.s_addr))[1],
                 ((uint8_t *)&(((struct sockaddr_in *)addr)->sin_addr.s_addr))[2],
                 ((uint8_t *)&(((struct sockaddr_in *)addr)->sin_addr.s_addr))[3]);

        APPL_LOG("%s", verbose_ipv4_buffer);
    } else {
        memcpy(((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, ((struct sockaddr_in6 *)result->ai_addr)->sin6_addr.s6_addr, 16);
        char verbose_ipv6_buffer[51];
        snprintf(verbose_ipv6_buffer,
                 sizeof(verbose_ipv6_buffer),
                 "DNS IPV6: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[0],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[1],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[2],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[3],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[4],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[5],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[6],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[7],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[8],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[9],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[10],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[11],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[12],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[13],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[14],
                 ((struct sockaddr_in6 *)addr)->sin6_addr.s6_addr[15]);
        APPL_LOG("%s", verbose_ipv6_buffer);
    }

    freeaddrinfo(result);
    APPL_LOG("DNS done");

    return 0;
}


/**@brief Helper function to parse the uri and save the remote to the LWM2M remote database. */
static uint32_t app_lwm2m_parse_uri_and_save_remote(uint16_t          short_server_id,
                                                    char            * server_uri,
                                                    bool            * secure,
                                                    struct sockaddr * p_remote)
{
    uint32_t err_code;

    // Use DNS to lookup the IP
    err_code = app_resolve_server_uri(server_uri, p_remote, secure, 0);

    if (err_code == 0)
    {
        // Deregister the short_server_id in case it has been registered with a different address
        (void) lwm2m_remote_deregister(short_server_id);

        // Register the short_server_id
        err_code = lwm2m_remote_register(short_server_id, (struct sockaddr *)p_remote);
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


/**@brief Helper function to handle a connect retry. */
void app_handle_connect_retry(int instance_id, bool no_reply)
{
    if (instance_id == 0 && m_server_settings[instance_id].retry_count == sizeof(app_retry_delay) - 1)
    {
        // Bootstrap retry does not use the last retry value and does not continue before next power up.
        m_app_state = APP_STATE_IP_INTERFACE_UP;
        m_server_settings[instance_id].retry_count = 0;
        APPL_LOG("Bootstrap procedure failed");
        return;
    }

    if (m_server_settings[instance_id].retry_count == sizeof app_retry_delay)
    {
        // Retry counter wrap around
        m_server_settings[instance_id].retry_count = 0;
    }

    bool start_retry_delay = true;

    if (no_reply)
    {
        // Fallback to the other IP version
        m_family_type[instance_id] = (m_family_type[instance_id] == AF_INET6) ? AF_INET : AF_INET6;

        if (m_family_type[instance_id] == AF_INET)
        {
            // No retry delay when IPv6 to IPv4 fallback
            APPL_LOG("IPv6 to IPv4 fallback");
            start_retry_delay = false;
        }
    }

    if (start_retry_delay)
    {
        s32_t retry_delay = app_retry_delay[m_server_settings[instance_id].retry_count];

        APPL_LOG("Retry delay for %d minutes...", retry_delay / 60);
        k_delayed_work_submit(&state_update_work, retry_delay * 1000);

        m_server_settings[instance_id].retry_count++;
    } else {
        k_delayed_work_submit(&state_update_work, 0);
    }
}


/**@brief LWM2M notification handler. */
void lwm2m_notification(lwm2m_notification_type_t type,
                        struct sockaddr *         p_remote,
                        uint8_t                   coap_code,
                        uint32_t                  err_code)
{
    if (IS_ENABLED(CONFIG_LOG)) {
        static char *str_type[] = { "Bootstrap", "Register", "Update", "Deregister" };
        APPL_LOG("Got LWM2M notifcation %s  CoAP %d.%02d  err:%lu", str_type[type], coap_code >> 5, coap_code & 0x1f, err_code);
    }

    if (type == LWM2M_NOTIFCATION_TYPE_BOOTSTRAP)
    {
        if (coap_code == COAP_CODE_204_CHANGED)
        {
            m_app_state = APP_STATE_BOOTSTRAPPING;
            APPL_LOG("Bootstrap timeout set to 20 seconds");
            k_delayed_work_submit(&state_update_work, 20 * 1000);
        }
        else if (coap_code == 0 || coap_code == COAP_CODE_403_FORBIDDEN)
        {
            // No response or received a 4.03 error.
            m_app_state = APP_STATE_BOOTSTRAP_WAIT;
            app_handle_connect_retry(0, false);
        }
        else
        {
            // TODO: What to do here?
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_REGISTER)
    {
        // Start lifetime timer
        s32_t timeout = (s32_t)(m_server_settings[m_server_instance].lifetime * 1000);
        if (timeout <= 0) {
            // FIXME: Lifetime timer too big for Zephyr, set to maximum possible value for now
            timeout = INT32_MAX;
        }
        k_delayed_work_submit(&connection_update_work[m_server_instance], timeout);

        if (coap_code == COAP_CODE_201_CREATED || coap_code == COAP_CODE_204_CHANGED)
        {
            printk("Registered %d\n", m_server_instance);
            m_server_settings[m_server_instance].retry_count = 0;
            m_server_settings[m_server_instance].is.registered = true;

            if (m_server_instance == 1 && m_server_settings[3].server_uri[0]) {
                m_app_state = APP_STATE_SERVER_CONNECT;
                m_server_instance = 3;
            } else {
                m_app_state = APP_STATE_SERVER_REGISTERED;
            }
        }
        else
        {
            m_app_state = APP_STATE_SERVER_REGISTER_WAIT;
            app_handle_connect_retry(m_server_instance, false);
        }
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_UPDATE)
    {
    }
    else if (type == LWM2M_NOTIFCATION_TYPE_DEREGISTER)
    {
        // We have successfully deregistered current server instance.
        m_server_settings[m_server_instance].is.registered = false;

        if (m_server_instance == 3) {
            m_app_state = APP_STATE_SERVER_DEREGISTER;
            m_server_instance = 1;
        } else {
            m_app_state = APP_STATE_DISCONNECT;
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
    s64_t time_stamp;
    s64_t milliseconds_spent;

    APPL_LOG("Bootstrap done, timeout cancelled");
    k_delayed_work_cancel(&state_update_work);

    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
    k_sleep(10); // TODO: figure out why this is needed before closing the connection

    // Close connection to bootstrap server.
    uint32_t err_code = coap_security_destroy(mp_lwm2m_bs_transport);
    APP_ERROR_CHECK(err_code);

    mp_lwm2m_bs_transport = NULL;

    m_app_state = APP_STATE_BOOTSTRAPPED;
    m_server_settings[0].retry_count = 0;

    time_stamp = k_uptime_get();

    app_provision_secret_keys();

    m_server_settings[0].is.bootstrapped = true;  // TODO: this should be set by bootstrap server when bootstrapped
    m_did_bootstrap = true;

#if CONFIG_FLASH
    APPL_LOG("Store bootstrap settings");
    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        app_store_bootstrap_server_acl(i);
        nvs_write(&fs, i, &m_server_settings[i], sizeof(m_server_settings[i]));
     }
#endif

    milliseconds_spent = k_uptime_delta(&time_stamp);

    m_app_state = APP_STATE_SERVER_CONNECT_WAIT;
    s32_t hold_off_time = (m_server_settings[m_server_instance].hold_off_timer * 1000) - milliseconds_spent;
    APPL_LOG("Client holdoff timer: sleeping %d milliseconds...", hold_off_time);
    k_delayed_work_submit(&state_update_work, hold_off_time);

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
                                                         &m_server_settings[instance_id].hold_off_timer);
                break;
            }
            case 1: // IsBootstrapped
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_server_settings[instance_id].is.bootstrapped);
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
    uint32_t err_code = 0;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_security_verizon_decode(instance_id, p_tlv);
            break;

        default:
#if 0
            err_code = ENOENT;
#else
            printk("Unhandled security resource: %i", p_tlv->id);
#endif
            break;
    }

    return err_code;
}


static uint32_t tlv_server_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    int32_t list_values[2] =
    {
        m_server_settings[instance_id].is.registered,
        m_server_settings[instance_id].hold_off_timer
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
                                                         &m_server_settings[instance_id].is.registered);
                break;
            }
            case 1: // ClientHoldOffTimer
            {
                err_code = lwm2m_tlv_bytebuffer_to_int32(tlv.value,
                                                         tlv.length,
                                                         &m_server_settings[instance_id].hold_off_timer);
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
    uint32_t err_code = 0;

    switch (p_tlv->id)
    {
        case VERIZON_RESOURCE:
            err_code = tlv_server_verizon_decode(instance_id, p_tlv);
            break;

        default:
#if 0
            err_code = ENOENT;
#else
            printk("Unhandled server resource: %i", p_tlv->id);
#endif
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
    uint32_t err_code = 0;

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
    APPL_LOG("server_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
                                                    p_instance,
                                                    p_request->remote);
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
        u32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                               p_request->payload,
                                               p_request->payload_len,
                                               resource_tlv_callback);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_server_decode(&m_instance_server[instance_id],
                                                      resource_id,
                                                      p_request->payload,
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
#if APP_MOTIVE_FIX_UPDATE_TRIGGER
                // Use instance_id 1 when MotiveBridge say /1/0/8
                if (instance_id == 0) {
                    instance_id = 1;
                }
#endif
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
    APPL_LOG("device_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
                                                    p_instance,
                                                    p_request->remote);
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
        u32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_device_decode(&m_instance_device,
                                               p_request->payload,
                                               p_request->payload_len,
                                               resource_tlv_callback);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_device_decode(&m_instance_device,
                                                      resource_id,
                                                      p_request->payload,
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
                app_disconnect();
#if APP_MOTIVE_NO_REBOOT
                m_app_state = APP_STATE_SERVER_CONNECT;
                m_server_instance = 1;
#else
                NVIC_SystemReset();
#endif
                break;
            }

            case LWM2M_DEVICE_FACTORY_RESET:
            {
                app_factory_reset();

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                // TODO: Shutdown and reboot
                app_disconnect();
#if APP_MOTIVE_NO_REBOOT
                m_app_state = APP_STATE_SERVER_CONNECT;
                m_server_instance = 1;
#else
                NVIC_SystemReset();
#endif
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
    APPL_LOG("conn_mon_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
                                                    p_instance,
                                                    p_request->remote);
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
        u32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_connectivity_monitoring_decode(&m_instance_conn_mon,
                                                                p_request->payload,
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
    else if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        APPL_LOG("CoAP observe requested on object 4/%i/%i", p_instance->instance_id, resource_id);

        u32_t observe_option = 0;
        for (uint8_t index = 0; index < p_request->options_count; index++)
        {
            if (p_request->options[index].number == COAP_OPT_OBSERVE)
            {
                err_code = coap_opt_uint_decode(&observe_option,
                                                p_request->options[index].length,
                                                p_request->options[index].data);
                break;
            }
        }

        if (err_code == 0)
        {
            if (observe_option == 0) // Observe start
            {
                APPL_LOG("CoAP observe requested on object 4/%i/%i - START", p_instance->instance_id, resource_id);
                uint8_t  buffer[200];
                uint32_t buffer_size = sizeof(buffer);
                err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                    &buffer_size,
                                                                    resource_id,
                                                                    &m_instance_conn_mon);

                err_code = lwm2m_observe_register(buffer,
                                                  buffer_size,
                                                  m_instance_conn_mon.proto.expire_time,
                                                  p_request,
                                                  COAP_CT_APP_LWM2M_TLV,
                                                  (lwm2m_instance_t *)&m_instance_conn_mon);
            }
            else // Observe stop
            {
                APPL_LOG("CoAP observe requested on object 4/%i/%i - STOP", p_instance->instance_id, resource_id);
            }
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

#if CONFIG_FLASH
    APPL_LOG("Store bootstrap security values");
    nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[instance_id]));
#endif

    return 0;
}


static uint32_t app_store_bootstrap_server_acl(uint16_t instance_id)
{
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *) &m_instance_server[instance_id].proto;
    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++)
    {
        m_server_settings[instance_id].access[i] = p_instance->acl.access[i];
        m_server_settings[instance_id].server[i] = p_instance->acl.server[i];
    }
    m_server_settings[instance_id].owner = p_instance->acl.owner;

    return 0;
}


static uint32_t app_store_bootstrap_server_values(uint16_t instance_id)
{
    if (m_instance_server[instance_id].binding.len >= SERVER_BINDING_SIZE_MAX)
    {
        // Binding was to long to be copied.
        return EINVAL;
    }

    if (m_server_settings[instance_id].lifetime != m_instance_server[instance_id].lifetime) {
        if (instance_id == 1 || instance_id == 3) {
            // Lifetime changed, send update server
            m_update_server = instance_id;
        }
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
    app_store_bootstrap_server_acl(instance_id);

#if CONFIG_FLASH
    APPL_LOG("Store bootstrap server values");
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
    APPL_LOG("server_object_callback");

    uint32_t err_code = 0;

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        (void)lwm2m_tlv_server_decode(&m_instance_server[instance_id],
                                      p_request->payload,
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


/**@brief Callback function for LWM2M security instances. */
uint32_t security_instance_callback(lwm2m_instance_t * p_instance,
                                    uint16_t           resource_id,
                                    uint8_t            op_code,
                                    coap_message_t *   p_request)
{
    APPL_LOG("security_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = app_lwm2m_access_remote_get(&access,
                                                    p_instance,
                                                    p_request->remote);
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
                                             p_request->payload,
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
    APPL_LOG("security_object_callback, instance %u", instance_id);

    if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t err_code = lwm2m_tlv_security_decode(&m_instance_security[instance_id],
                                                      p_request->payload,
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

        APPL_LOG("Secret key %d: %s", instance_id, m_secret_key[instance_id]);

        APPL_LOG("decoded security.");

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
            m_server_settings[0].is.bootstrapped = 0;
            m_server_settings[0].hold_off_timer = 10;

            app_provision_psk(APP_BOOTSTRAP_SEC_TAG, app_client_imei_msisdn(), APP_BOOTSTRAP_SEC_PSK);

            break;
        }

        case 1: // DM server
        {
            memset(&m_server_settings[1], 0, sizeof(m_server_settings[1]));
            m_server_settings[1].access[0] = rwde_access;
            m_server_settings[1].server[0] = 101;
            m_server_settings[1].access[1] = rwde_access;
            m_server_settings[1].server[1] = 102;
            m_server_settings[1].access[2] = rwde_access;
            m_server_settings[1].server[2] = 1000;
            m_server_settings[1].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
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
            m_server_settings[2].hold_off_timer = 30;
            break;
        }

        case 3: // Repository server
        {
            memset(&m_server_settings[3], 0, sizeof(m_server_settings[3]));
            m_server_settings[3].access[0] = rwde_access;
            m_server_settings[3].server[0] = 101;
            m_server_settings[3].access[1] = rwde_access;
            m_server_settings[3].server[1] = 102;
            m_server_settings[3].access[2] = rwde_access;
            m_server_settings[3].server[2] = 1000;
            m_server_settings[3].owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
            break;
        }

        default:
            break;
    }
}


static void app_init_device_settings(void)
{
    strcpy(m_device_settings.imei, IMEI);
    strcpy(m_device_settings.msisdn, MSISDN);
    strcpy(m_device_settings.manufacturer, "Nordic Semiconductor ASA");
    strcpy(m_device_settings.model_number, "nRF9160");
    strcpy(m_device_settings.serial_number, "1234567890");
    strcpy(m_device_settings.modem_logging, "1");
}


static void app_factory_reset(void)
{
#if CONFIG_FLASH
        for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
        {
            nvs_delete(&fs, i);
        }
#endif
}


static void app_read_flash_storage(void)
{
#if CONFIG_FLASH
    int rc;

    rc = nvs_read(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));
    if (rc <= 0) {
        app_init_device_settings();
        nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));
    }

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        rc = nvs_read(&fs, i, &m_server_settings[i], sizeof(m_server_settings[i]));
        if (rc <= 0) // TODO: check if written size is correct?
        {
            // Not found, create new factory bootstrapped object.
            app_factory_bootstrap_server_object(i);

            if (m_server_settings[i].short_server_id)
            {
                // Write settings for initialized server objects.
                rc = nvs_write(&fs, i, &m_server_settings[i], sizeof(m_server_settings[i]));
                if (rc <= 0)
                {
                    APPL_LOG("Storing server settings failed");
                }
            }
        }
    }
#else
    app_init_device_settings();

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        app_factory_bootstrap_server_object(i);
    }

#if CONFIG_DK_LIBRARY
    // Workaround for not storing is.bootstrapped:
    // - Switch 1 will determine if doing bootstrap

    u32_t button_state = 0;
    dk_read_buttons(&button_state, NULL);

    if (button_state & 0x04) // Switch 1 in left position
    {
        m_server_settings[0].is.bootstrapped = false;
    }
    else
    {
        m_server_settings[0].is.bootstrapped = true;
    }
#endif

    // Bootstrap values (will be fetched from NVS after bootstrap)
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);


    // DM server
    m_server_settings[1].short_server_id = 102;
#if APP_USE_CONTABO
    strcpy(m_server_settings[1].server_uri, "coap://vmi36865.contabo.host:5683");
#else
    strcpy(m_server_settings[1].server_uri, "coaps://ddocdp.do.motive.com:5684");
#endif
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
    m_server_settings[1].hold_off_timer = 30;


    if (m_server_settings[0].is.bootstrapped)
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
    m_server_settings[3].hold_off_timer = 30;
#endif
}


static void app_lwm2m_create_objects(void)
{
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

    m_instance_device.manufacturer.p_val = m_device_settings.manufacturer;
    m_instance_device.manufacturer.len = strlen(m_instance_device.manufacturer.p_val);
    m_instance_device.model_number.p_val = m_device_settings.model_number;
    m_instance_device.model_number.len = strlen(m_instance_device.model_number.p_val);
    m_instance_device.serial_number.p_val = m_device_settings.serial_number;
    m_instance_device.serial_number.len = strlen(m_instance_device.serial_number.p_val);
    m_instance_device.firmware_version.p_val = "1.0";
    m_instance_device.firmware_version.len = strlen(m_instance_device.firmware_version.p_val);

#if APP_MOTIVE_FAKE_POWER_SOURCES
    m_instance_device.avail_power_sources.len = 2;
    m_instance_device.avail_power_sources.val.p_uint8[0] = 0; // DC power
    m_instance_device.avail_power_sources.val.p_uint8[1] = 2; // External Battery
    m_instance_device.power_source_voltage.len = 2;
    m_instance_device.power_source_voltage.val.p_int32[0] = 5108;
    m_instance_device.power_source_voltage.val.p_int32[1] = 5242;
    m_instance_device.power_source_current.len = 2;
    m_instance_device.power_source_current.val.p_int32[0] = 42;
    m_instance_device.power_source_current.val.p_int32[1] = 0;
#else
    m_instance_device.avail_power_sources.len = 0;
    m_instance_device.power_source_voltage.len = 0;
    m_instance_device.power_source_current.len = 0;
#endif

    m_instance_device.battery_level = 0;
    m_instance_device.memory_free = 64;
    m_instance_device.error_code.len = 1;
    m_instance_device.error_code.val.p_int32[0] = 0; // No error
    m_instance_device.current_time = 1546300800; // Tue Jan 01 00:00:00 CEST 2019
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
    m_instance_device.battery_status = 5;
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
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    102);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_device);

    //
    // Connectivity Monitoring instance.
    //
    lwm2m_instance_connectivity_monitoring_init(&m_instance_conn_mon);

    m_instance_conn_mon.proto.expire_time = 60; // Default to 60 second notifications.
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
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    102);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_conn_mon,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    1000);

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
    memcpy(&m_client_id.value.imei_msisdn[0], app_client_imei_msisdn(), LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN);
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
                                              &m_bs_remote_server);

    if (secure == true)
    {
        APPL_LOG("SECURE session (bootstrap)");

        struct sockaddr local_addr;
        app_init_sockaddr_in(&local_addr, m_bs_remote_server.sa_family, LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT);

        #define SEC_TAG_COUNT 1

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = {APP_BOOTSTRAP_SEC_TAG};

        coap_sec_config_t setting =
        {
            .role          = 0,    // 0 -> Client role
            .sec_tag_count = SEC_TAG_COUNT,
            .sec_tag_list  = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr     = &local_addr,
            .setting  = &setting,
            .protocol = IPPROTO_DTLS_1_2
        };

        // NOTE: This method initiates a DTLS handshake and may block for a some seconds.
        err_code = coap_security_setup(&local_port, &m_bs_remote_server);

        if (err_code == 0)
        {
            m_app_state = APP_STATE_BS_CONNECTED;
            mp_lwm2m_bs_transport = local_port.transport;
        }
        else
        {
            m_app_state = APP_STATE_BS_CONNECT_WAIT;
            // Check for no IPv6 support (EINVAL) and no response (ENETUNREACH)
            if (err_code == EIO && (errno == EINVAL || errno == ENETUNREACH)) {
                app_handle_connect_retry(0, true);
            } else {
                app_handle_connect_retry(0, false);
            }
        }
    }
    else
    {
        APPL_LOG("NON-SECURE session (bootstrap)");
    }
}


static void app_bootstrap(void)
{
    uint32_t err_code = lwm2m_bootstrap((struct sockaddr *)&m_bs_remote_server, &m_client_id, mp_lwm2m_bs_transport);
    if (err_code == 0)
    {
        m_app_state = APP_STATE_BOOTSTRAP_REQUESTED;
    }
}


static void app_server_connect(void)
{
    uint32_t err_code;
    bool secure;

    // Initialize server configuration structure.
    memset(&m_server_conf[m_server_instance], 0, sizeof(lwm2m_server_config_t));
    m_server_conf[m_server_instance].lifetime = m_server_settings[m_server_instance].lifetime;

    // Set the short server id of the server in the config.
    m_server_conf[m_server_instance].short_server_id = m_server_settings[m_server_instance].short_server_id;

    err_code = app_resolve_server_uri((char *)&m_server_settings[m_server_instance].server_uri,
                                      &m_remote_server[m_server_instance], &secure, m_server_instance);
    if (err_code != 0)
    {
        app_handle_connect_retry(m_server_instance, true);
        return;
    }

    if (secure == true)
    {
        APPL_LOG("SECURE session (register)");

        struct sockaddr local_addr;
        app_init_sockaddr_in(&local_addr, m_remote_server[m_server_instance].sa_family, LWM2M_LOCAL_CLIENT_PORT_OFFSET + m_server_instance);

        #define SEC_TAG_COUNT 1

        sec_tag_t sec_tag_list[SEC_TAG_COUNT] = { APP_SEC_TAG_OFFSET + m_server_instance };

        coap_sec_config_t setting =
        {
            .role           = 0,    // 0 -> Client role
            .sec_tag_count  = SEC_TAG_COUNT,
            .sec_tag_list = sec_tag_list
        };

        coap_local_t local_port =
        {
            .addr    = &local_addr,
            .setting = &setting,
            .protocol  = IPPROTO_DTLS_1_2
        };

        // NOTE: This method initiates a DTLS handshake and may block for some seconds.
        err_code = coap_security_setup(&local_port, &m_remote_server[m_server_instance]);

        if (err_code == 0)
        {
            m_app_state = APP_STATE_SERVER_CONNECTED;
            mp_lwm2m_transport[m_server_instance] = local_port.transport;
            m_server_settings[m_server_instance].retry_count = 0;
        }
        else
        {
            m_app_state = APP_STATE_SERVER_CONNECT_WAIT;
            // Check for no IPv6 support (EINVAL) and no response (ENETUNREACH)
            if (err_code == EIO && (errno == EINVAL || errno == ENETUNREACH)) {
                app_handle_connect_retry(m_server_instance, true);
            } else {
                app_handle_connect_retry(m_server_instance, false);
            }
        }
    }
    else
    {
        APPL_LOG("NON-SECURE session (register)");
        m_app_state = APP_STATE_SERVER_CONNECTED;
    }
}


static void app_server_register(void)
{
    uint32_t err_code;
    uint32_t link_format_string_len = 0;

    // Dry run the link format generation, to check how much memory that is needed.
    err_code = lwm2m_coap_handler_gen_link_format(NULL, (uint16_t *)&link_format_string_len);
    APP_ERROR_CHECK(err_code);

    // Allocate the needed amount of memory.
    uint8_t * p_link_format_string = k_malloc(link_format_string_len);

    if (p_link_format_string != NULL)
    {
        // Render the link format string.
        err_code = lwm2m_coap_handler_gen_link_format(p_link_format_string, (uint16_t *)&link_format_string_len);
        APP_ERROR_CHECK(err_code);

        err_code = lwm2m_register((struct sockaddr *)&m_remote_server[m_server_instance],
                                  &m_client_id,
                                  &m_server_conf[m_server_instance],
                                  mp_lwm2m_transport[m_server_instance],
                                  p_link_format_string,
                                  (uint16_t)link_format_string_len);
        APP_ERROR_CHECK(err_code);

        m_app_state = APP_STATE_SERVER_REGISTER_WAIT;
        k_free(p_link_format_string);
    }
}


static void app_server_update(uint16_t instance_id)
{
    uint32_t err_code;

    err_code = lwm2m_update((struct sockaddr *)&m_remote_server[instance_id],
                            &m_server_conf[instance_id],
                            mp_lwm2m_transport[instance_id]);
    ARG_UNUSED(err_code);

    // Restart lifetime timer
    s32_t timeout = (s32_t)(m_server_settings[instance_id].lifetime * 1000);
    if (timeout <= 0) {
        // FIXME: Lifetime timer too big for Zephyr, set to maximum possible value for now
        timeout = INT32_MAX;
    }

    k_delayed_work_submit(&connection_update_work[instance_id], timeout);
}


static void app_server_deregister(uint16_t instance_id)
{
    uint32_t err_code;

    err_code = lwm2m_deregister((struct sockaddr *)&m_remote_server[instance_id],
                                mp_lwm2m_transport[instance_id]);
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
        ARG_UNUSED(err_code);

        mp_lwm2m_bs_transport = NULL;
    }

    for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
        if (mp_lwm2m_transport[i])
        {
            err_code = coap_security_destroy(mp_lwm2m_transport[i]);
            ARG_UNUSED(err_code);

            mp_lwm2m_transport[i] = NULL;
        }
    }

    m_app_state = APP_STATE_IP_INTERFACE_UP;
}


static void app_wait_state_update(struct k_work *work)
{
    ARG_UNUSED(work);

    switch(m_app_state)
    {
        case APP_STATE_BS_CONNECT_WAIT:
            // Timeout waiting for DTLS connection to bootstrap server
            m_app_state = APP_STATE_BS_CONNECT;
            break;

        case APP_STATE_BOOTSTRAP_WAIT:
            // Timeout waiting for bootstrap ACK (CoAP)
            m_app_state = APP_STATE_BS_CONNECTED;
            break;

        case APP_STATE_BOOTSTRAPPING:
            // Timeout waiting for bootstrap to finish
            m_app_state = APP_STATE_BS_CONNECT_WAIT;
            app_handle_connect_retry(0, false);
            break;

        case APP_STATE_SERVER_CONNECT_WAIT:
            // Timeout waiting for DTLS connection to registration server
            m_app_state = APP_STATE_SERVER_CONNECT;
            break;

        case APP_STATE_SERVER_REGISTER_WAIT:
            // Timeout waiting for registration ACK (CoAP)
            m_app_state = APP_STATE_SERVER_CONNECTED;
            break;

        default:
            // Unknown timeout state
            break;
    }
}


static void app_lwm2m_process(void)
{
    coap_input();

    switch(m_app_state)
    {
        case APP_STATE_BS_CONNECT:
        {
            APPL_LOG("app_bootstrap_connect");
            if (mp_lwm2m_bs_transport)
            {
                // Already connected. Disconnect first.
                app_disconnect();
            }
            app_bootstrap_connect();
            break;
        }
        case APP_STATE_BS_CONNECTED:
        {
            APPL_LOG("app_bootstrap");
            app_bootstrap();
            break;
        }
        case APP_STATE_SERVER_CONNECT:
        {
            APPL_LOG("app_server_connect, \"%s server\"", (m_server_instance == 1) ? "DM" : "Repository");
            app_server_connect();
            break;
        }
        case APP_STATE_SERVER_CONNECTED:
        {
            APPL_LOG("app_server_register");
            app_server_register();
            break;
        }
        case APP_STATE_SERVER_DEREGISTER:
        {
            APPL_LOG("app_server_deregister");
            app_server_deregister(m_server_instance);
            break;
        }
        case APP_STATE_DISCONNECT:
        {
            APPL_LOG("app_disconnect");
            app_disconnect();
            break;
        }
        default:
        {
            if (m_update_server > 0)
            {
                APPL_LOG("app_server_update");
                app_server_update(m_update_server);
                m_update_server = 0;
            }
            break;
        }
    }
}

static void app_coap_init(void)
{
    uint32_t err_code;

    struct sockaddr local_addr;
    struct sockaddr non_sec_local_addr;
    app_init_sockaddr_in(&local_addr, AF_INET, COAP_LOCAL_LISTENER_PORT);
    app_init_sockaddr_in(&non_sec_local_addr, AF_INET, LWM2M_LOCAL_LISTENER_PORT);

    // If bootstrap server and server is using different port we can
    // register the ports individually.
    coap_local_t local_port_list[COAP_PORT_COUNT] =
    {
        {
            .addr = &local_addr
        },
        {
            .addr = &non_sec_local_addr,
            .protocol = IPPROTO_UDP,
            .setting = NULL,
        }
    };

    // Verify that the port count defined in sdk_config.h is matching the one configured for coap_init.
    APP_ERROR_CHECK_BOOL(((sizeof(local_port_list)) / (sizeof(coap_local_t))) == COAP_PORT_COUNT);

    coap_transport_init_t port_list;
    port_list.port_table = &local_port_list[0];

    err_code = coap_init(17, &port_list, k_malloc, k_free);
    APP_ERROR_CHECK(err_code);

    mp_coap_transport = local_port_list[0].transport;
    mp_lwm2m_transport[1] = local_port_list[1].transport;
    ARG_UNUSED(mp_coap_transport);
    ARG_UNUSED(mp_lwm2m_transport);
}


static void app_provision_psk(int sec_tag, char * identity, char * psk)
{
    uint32_t err_code;

    err_code = nrf_inbuilt_key_write(sec_tag,
                                     NRF_KEY_MGMT_CRED_TYPE_IDENTITY,
                                     identity, strlen(identity));
    APP_ERROR_CHECK(err_code);

    err_code = nrf_inbuilt_key_write(sec_tag,
                                     NRF_KEY_MGMT_CRED_TYPE_PSK,
                                     psk, strlen(psk));
    APP_ERROR_CHECK(err_code);
}


static void app_provision_secret_keys(void)
{
    APPL_LOG(">> %s", __func__);

    lte_lc_offline();
    APPL_LOG("Offline mode");

    for (uint32_t i = 0; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        if (m_public_key[i] && m_secret_key[i])
        {
            static char server_uri_val[SECURITY_SERVER_URI_SIZE_MAX];
            strcpy(server_uri_val, (char *)&m_server_settings[i].server_uri);

            bool secure = false;
            uint16_t port = 0;
            const char * hostname = app_uri_get(server_uri_val, &port, &secure);
            (void)hostname;

            if (secure) {
                APPL_LOG("Provisioning key for %s, short-id: %u", server_uri_val, m_server_settings[i].short_server_id);
                app_provision_psk(APP_SEC_TAG_OFFSET + i, m_public_key[i], m_secret_key[i]);
            }
            k_free(m_public_key[i]);
            m_public_key[i] = NULL;
            k_free(m_secret_key[i]);
            m_secret_key[i] = NULL;
        }
    }
    APPL_LOG("Wrote secret keys");

    lte_lc_normal();

    // THIS IS A HACK. Temporary solution to give a delay to recover Non-DTLS sockets from CFUN=4.
    // The delay will make TX available after CID again set.
    k_sleep(K_MSEC(2000));

    APPL_LOG("Normal mode");
}


/**@brief Initialize Non-volatile Storage. */
static void app_flash_init(void)
{
#if CONFIG_FLASH
    int rc = nvs_init(&fs, DT_FLASH_DEV_NAME);
    if (rc) {
        APPL_LOG("Flash init failed: %d", rc);
    }
#endif
}


static void send_at_command(const char *at_command, bool do_logging)
{
#define APP_MAX_AT_READ_LENGTH          100
#define APP_MAX_AT_WRITE_LENGTH         256

    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return;
    }

    if (do_logging) {
        printk("send: %s\n", at_command);
    }

    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_command);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            if (do_logging) {
                printk("recv: %s\n", read_buffer);
            }
        } else {
            printk("recv() failed\n");
        }
    } else {
        printk("send() failed\n");
    }

    close(at_socket_fd);
}


static void modem_trace_enable(void)
{
    /* GPIO configurations for trace and debug */
    #define CS_PIN_CFG_TRACE_CLK    21 //GPIO_OUT_PIN21_Pos
    #define CS_PIN_CFG_TRACE_DATA0  22 //GPIO_OUT_PIN22_Pos
    #define CS_PIN_CFG_TRACE_DATA1  23 //GPIO_OUT_PIN23_Pos
    #define CS_PIN_CFG_TRACE_DATA2  24 //GPIO_OUT_PIN24_Pos
    #define CS_PIN_CFG_TRACE_DATA3  25 //GPIO_OUT_PIN25_Pos

    // Configure outputs.
    // CS_PIN_CFG_TRACE_CLK
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_CLK] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                               (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA0
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA0] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA1
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA1] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA2
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA2] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA3
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA3] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    NRF_P0_NS->DIR = 0xFFFFFFFF;
}


#if CONFIG_DK_LIBRARY
/**@brief Check buttons pressed at startup. */
void app_check_buttons_pressed(void)
{
    u32_t button_state = 0;
    dk_read_buttons(&button_state, NULL);

    // Check if button 1 pressed during startup
    if (button_state & 0x01) {
        app_factory_reset();

        printk("Factory reset!\n");
        k_delayed_work_cancel(&leds_update_work);
        while (true) { // Blink all LEDs
            dk_set_leds_state(DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK, 0);
            k_sleep(250);
            dk_set_leds_state(0, DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK);
            k_sleep(250);
        }
    }
}
#endif


#ifdef CONFIG_SHELL
static int cmd_at_command(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"AT command\"", argv[0]);
        return 0;
    }

    send_at_command(argv[1], true);

    return 0;
}

#if CONFIG_FLASH
static int cmd_config_clear(const struct shell *shell, size_t argc, char **argv)
{
    m_server_settings[0].is.bootstrapped = false;
    nvs_write(&fs, 0, &m_server_settings[0], sizeof(m_server_settings[0]));

    app_factory_bootstrap_server_object(1);
    nvs_delete(&fs, 1);

    app_factory_bootstrap_server_object(3);
    nvs_delete(&fs, 3);

    shell_print(shell, "Deleted all bootstrapped values");
    return 0;
}


static int cmd_config_print(const struct shell *shell, size_t argc, char **argv)
{
    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++)
    {
        if (m_server_settings[i].short_server_id) {
            shell_print(shell, "Instance %d", i);
            shell_print(shell, "  Short Server ID  %d", m_server_settings[i].short_server_id);
            shell_print(shell, "  Server URI       %s", m_server_settings[i].server_uri);
            shell_print(shell, "  Lifetime         %lld", m_server_settings[i].lifetime);
            shell_print(shell, "  Owner            %d", m_server_settings[i].owner);
        }
    }

    return 0;
}


static int cmd_config_uri(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_print(shell, "%s <instance> <URI>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);
    char *uri = argv[2];
    size_t uri_len = strlen(uri);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (uri_len > SECURITY_SERVER_URI_SIZE_MAX)
    {
        shell_print(shell, "maximum URI length is %d", SECURITY_SERVER_URI_SIZE_MAX);
        return 0;
    }

    memset(m_server_settings[instance_id].server_uri, 0, SECURITY_SERVER_URI_SIZE_MAX);
    strcpy(m_server_settings[instance_id].server_uri, uri);
    nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[0]));

    shell_print(shell, "Set URI %d: %s", instance_id, uri);

    return 0;
}


static int cmd_config_lifetime(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_print(shell, "%s <instance> <seconds>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);
    time_t lifetime = (time_t) atoi(argv[2]);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (lifetime != m_server_settings[instance_id].lifetime) {
        if (instance_id == 1 || instance_id == 3) {
            // Lifetime changed, send update server
            m_update_server = instance_id;
        }

        m_server_settings[instance_id].lifetime = lifetime;
        m_instance_server[instance_id].lifetime = lifetime;
        m_server_conf[instance_id].lifetime = lifetime;

        nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[0]));

        shell_print(shell, "Set lifetime %d: %d", instance_id, lifetime);
    }

    return 0;
}

static int cmd_config_owner(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_print(shell, "%s <instance> <owner>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);
    uint16_t owner = (uint16_t) atoi(argv[2]);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (owner != m_server_settings[instance_id].owner) {

        m_server_settings[instance_id].owner = owner;
        m_instance_server[instance_id].proto.acl.owner = owner;

        nvs_write(&fs, instance_id, &m_server_settings[instance_id], sizeof(m_server_settings[0]));

        shell_print(shell, "Set owner %d: %d", instance_id, owner);
    }

    return 0;
}


static int cmd_device_print(const struct shell *shell, size_t argc, char **argv)
{
    extern char imei[];
    extern char msisdn[];

    shell_print(shell, "Device configuration");
    shell_print(shell, "  Manufacturer   %s", m_device_settings.manufacturer);
    shell_print(shell, "  Model number   %s", m_device_settings.model_number);
    shell_print(shell, "  Serial number  %s", m_device_settings.serial_number);
    if (m_device_settings.imei[0]) {
        shell_print(shell, "  IMEI           %s (static)", m_device_settings.imei);
    } else {
        shell_print(shell, "  IMEI           %s", imei);
    }
    if (m_device_settings.msisdn[0]) {
        shell_print(shell, "  MSISDN         %s (static)", m_device_settings.msisdn);
    } else {
        shell_print(shell, "  MSISDN         %s", msisdn);
    }
    shell_print(shell, "  Logging        %s", m_device_settings.modem_logging);

    return 0;
}


static int cmd_device_reset(const struct shell *shell, size_t argc, char **argv)
{
    app_init_device_settings();
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    return 0;
}


static int cmd_device_manufacturer(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"Manufacturer\"", argv[0]);
        return 0;
    }

    char *manufacturer = argv[1];
    size_t manufacturer_len = strlen(manufacturer);

    if (manufacturer_len > sizeof(m_device_settings.manufacturer))
    {
        shell_print(shell, "maximum manufacturer length is %d", sizeof(m_device_settings.manufacturer));
        return 0;
    }

    memset(m_device_settings.manufacturer, 0, sizeof(m_device_settings.manufacturer));
    strcpy(m_device_settings.manufacturer, manufacturer);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    shell_print(shell, "Set manufacturer: %s", manufacturer);

    return 0;
}


static int cmd_device_model_number(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"Model number\"", argv[0]);
        return 0;
    }

    char *model = argv[1];
    size_t model_len = strlen(model);

    if (model_len > sizeof(m_device_settings.model_number))
    {
        shell_print(shell, "maximum model number length is %d", sizeof(m_device_settings.model_number));
        return 0;
    }

    memset(m_device_settings.model_number, 0, sizeof(m_device_settings.model_number));
    strcpy(m_device_settings.model_number, model);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    shell_print(shell, "Set model number: %s", model);

    return 0;
}


static int cmd_device_serial_number(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"Serial number\"", argv[0]);
        return 0;
    }

    char *serial = argv[1];
    size_t serial_len = strlen(serial);

    if (serial_len > sizeof(m_device_settings.serial_number))
    {
        shell_print(shell, "maximum serial number length is %d", sizeof(m_device_settings.serial_number));
        return 0;
    }

    memset(m_device_settings.serial_number, 0, sizeof(m_device_settings.serial_number));
    strcpy(m_device_settings.serial_number, serial);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    shell_print(shell, "Set serial number: %s", serial);

    return 0;
}


static int cmd_device_imei(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s IMEI", argv[0]);
        return 0;
    }

    char *imei = argv[1];
    size_t imei_len = strlen(imei);

    if (imei_len != 0 && imei_len != 15) {
        shell_print(shell, "length of IMEI must be 15");
        return 0;
    }

    memset(m_device_settings.imei, 0, sizeof(m_device_settings.imei));
    strcpy(m_device_settings.imei, imei);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    if (imei_len) {
        shell_print(shell, "Set IMEI: %s", imei);
    } else {
        shell_print(shell, "Removed IMEI");
    }

    return 0;
}


static int cmd_device_msisdn(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s MSISDN", argv[0]);
        return 0;
    }

    char *msisdn = argv[1];
    size_t msisdn_len = strlen(msisdn);

    if (msisdn_len != 0 && msisdn_len != 10) {
        shell_print(shell, "length of MSISDN must be 10");
        return 0;
    }

    memset(m_device_settings.msisdn, 0, sizeof(m_device_settings.msisdn));
    strcpy(m_device_settings.msisdn, msisdn);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    if (msisdn_len) {
        shell_print(shell, "Set MSISDN: %s", msisdn);
    } else {
        shell_print(shell, "Removed MSISDN");
    }

    return 0;
}


static int cmd_device_logging(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        return 0;
    }

    char *logging = argv[1];
    size_t logging_len = strlen(logging);

    if (logging_len != 1 && logging_len != 64) {
        shell_print(shell, "invalid logging value");
        return 0;
    }

    memset(m_device_settings.modem_logging, 0, sizeof(m_device_settings.modem_logging));
    strcpy(m_device_settings.modem_logging, logging);
    nvs_write(&fs, DEVICE_FLASH_ID, &m_device_settings, sizeof(m_device_settings));

    shell_print(shell, "Set logging value: %s", logging);

    return 0;
}


#endif


static int cmd_lwm2m_register(const struct shell *shell, size_t argc, char **argv)
{
    if (m_app_state == APP_STATE_IP_INTERFACE_UP) {
        if (m_server_settings[0].is.bootstrapped) {
            m_app_state = APP_STATE_SERVER_CONNECT;
        } else {
            m_app_state = APP_STATE_BS_CONNECT;
        }
    } else if (m_app_state == APP_STATE_SERVER_REGISTERED) {
        shell_print(shell, "Already registered");
    } else {
        shell_print(shell, "Wrong state for registration");
    }

    return 0;
}


static int cmd_lwm2m_update(const struct shell *shell, size_t argc, char **argv)
{
    uint16_t instance_id = 1;

    if (argc == 2) {
        instance_id = atoi(argv[1]);

        if (instance_id != 1 && instance_id != 3)
        {
            shell_print(shell, "instance must be 1 or 3");
            return 0;
        }
    }

    if (m_app_state == APP_STATE_SERVER_REGISTERED) {
        m_update_server = instance_id;
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_deregister(const struct shell *shell, size_t argc, char **argv)
{
    if (m_app_state == APP_STATE_SERVER_REGISTERED) {
        m_app_state = APP_STATE_SERVER_DEREGISTER;
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_status(const struct shell *shell, size_t argc, char **argv)
{
    char ip_version[] = "IPvX";
    ip_version[3] = (m_family_type[m_server_instance] == AF_INET6) ? '6' : '4';

    if (m_did_bootstrap) {
        shell_print(shell, "Bootstrap completed [%s]", (m_family_type[0] == AF_INET6) ? "IPv6" : "IPv4");
    }

    if (m_server_instance == 3) {
        shell_print(shell, "Server 1 registered [%s]", (m_family_type[1] == AF_INET6) ? "IPv6" : "IPv4");
    }

    switch(m_app_state)
    {
        case APP_STATE_IDLE:
            shell_print(shell, "Idle");
            break;
        case APP_STATE_IP_INTERFACE_UP:
            shell_print(shell, "Disconnected");
            break;
        case APP_STATE_BS_CONNECT:
            shell_print(shell, "Bootstrap connect [%s]", ip_version);
            break;
        case APP_STATE_BS_CONNECT_WAIT:
            if (m_server_settings[0].retry_count > 0) {
                shell_print(shell, "Bootstrap retry delay (%d minutes) [%s]", app_retry_delay[m_server_settings[0].retry_count - 1] / 60, ip_version);
            } else {
                shell_print(shell, "Bootstrap connect wait [%s]", ip_version);
            }
            break;
        case APP_STATE_BS_CONNECTED:
            shell_print(shell, "Bootstrap connected [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAP_REQUESTED:
            shell_print(shell, "Bootstrap requested [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAP_WAIT:
            if (m_server_settings[0].retry_count > 0) {
                shell_print(shell, "Bootstrap delay (%d minutes) [%s]", app_retry_delay[m_server_settings[0].retry_count - 1] / 60, ip_version);
            } else {
                shell_print(shell, "Bootstrap wait [%s]", ip_version);
            }
            break;
        case APP_STATE_BOOTSTRAPPING:
            shell_print(shell, "Bootstrapping [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAPPED:
            shell_print(shell, "Bootstrapped [%s]", ip_version);
            break;
        case APP_STATE_SERVER_CONNECT:
            shell_print(shell, "Server %d connect [%s]", m_server_instance, ip_version);
            break;
        case APP_STATE_SERVER_CONNECT_WAIT:
            if (m_server_settings[m_server_instance].retry_count > 0) {
                shell_print(shell, "Server %d retry delay (%d minutes) [%s]", m_server_instance, app_retry_delay[m_server_settings[m_server_instance].retry_count - 1] / 60, ip_version);
            } else {
                shell_print(shell, "Server %d connect wait [%s]", m_server_instance, ip_version);
            }
            break;
        case APP_STATE_SERVER_CONNECTED:
            shell_print(shell, "Server %d connected [%s]", m_server_instance, ip_version);
            break;
        case APP_STATE_SERVER_REGISTER_WAIT:
            if (m_server_settings[m_server_instance].retry_count > 0) {
                shell_print(shell, "Server %d register delay (%d minutes) [%s]", m_server_instance, app_retry_delay[m_server_settings[m_server_instance].retry_count - 1] / 60, ip_version);
            } else {
                shell_print(shell, "Server %d register wait [%s]", m_server_instance, ip_version);
            }
            break;
        case APP_STATE_SERVER_REGISTERED:
            shell_print(shell, "Server %d registered [%s]", m_server_instance, ip_version);
            break;
        case APP_STATE_SERVER_DEREGISTER:
            shell_print(shell, "Server deregister");
            break;
        case APP_STATE_SERVER_DEREGISTERING:
            shell_print(shell, "Server deregistering");
            break;
        case APP_STATE_DISCONNECT:
            shell_print(shell, "Disconnect");
            break;
        default:
            shell_print(shell, "Unknown state: %d", m_app_state);
            break;
    };

    return 0;
}


static int cmd_factory_reset(const struct shell *shell, size_t argc, char **argv)
{
    app_factory_reset();
    NVIC_SystemReset();

    return 0;
}


static int cmd_reboot(const struct shell *shell, size_t argc, char **argv)
{
    NVIC_SystemReset();

    return 0;
}


#if CONFIG_FLASH
SHELL_CREATE_STATIC_SUBCMD_SET(sub_config)
{
    SHELL_CMD(print, NULL, "Print configuration", cmd_config_print),
    SHELL_CMD(clear, NULL, "Clear bootstrapped values", cmd_config_clear),
    SHELL_CMD(uri, NULL, "Set URI", cmd_config_uri),
    SHELL_CMD(lifetime, NULL, "Set lifetime", cmd_config_lifetime),
    SHELL_CMD(owner, NULL, "Set access control owner", cmd_config_owner),
    SHELL_CMD(factory_reset, NULL, "Factory reset", cmd_factory_reset),
    SHELL_SUBCMD_SET_END /* Array terminated. */
};

SHELL_CREATE_STATIC_SUBCMD_SET(sub_device)
{
    SHELL_CMD(print, NULL, "Print configuration", cmd_device_print),
    SHELL_CMD(reset, NULL, "Reset configuration", cmd_device_reset),
    SHELL_CMD(manufacturer, NULL, "Set manufacturer", cmd_device_manufacturer),
    SHELL_CMD(model_number, NULL, "Set model number", cmd_device_model_number),
    SHELL_CMD(serial_number, NULL, "Set serial number", cmd_device_serial_number),
    SHELL_CMD(imei, NULL, "Set IMEI", cmd_device_imei),
    SHELL_CMD(msisdn, NULL, "Set MSISDN", cmd_device_msisdn),
    SHELL_CMD(logging, NULL, "Set logging value", cmd_device_logging),
    SHELL_SUBCMD_SET_END /* Array terminated. */
};
#endif


SHELL_CREATE_STATIC_SUBCMD_SET(sub_lwm2m)
{
    SHELL_CMD(status, NULL, "Application status", cmd_lwm2m_status),
    SHELL_CMD(register, NULL, "Register server", cmd_lwm2m_register),
    SHELL_CMD(update, NULL, "Update server", cmd_lwm2m_update),
    SHELL_CMD(deregister, NULL, "Deregister server", cmd_lwm2m_deregister),
    SHELL_SUBCMD_SET_END /* Array terminated. */
};


SHELL_CMD_REGISTER(at, NULL, "Send AT command", cmd_at_command);
#if CONFIG_FLASH
SHELL_CMD_REGISTER(config, &sub_config, "Instance configuration", NULL);
SHELL_CMD_REGISTER(device, &sub_device, "Device configuration", NULL);
#endif
SHELL_CMD_REGISTER(lwm2m, &sub_lwm2m, "LwM2M operations", NULL);
SHELL_CMD_REGISTER(reboot, NULL, "Reboot", cmd_reboot);
#endif


/**@brief Handle server lifetime.
 */
static void app_connection_update(struct k_work *work)
{
    if (m_app_state == APP_STATE_SERVER_REGISTERED) {
        for (int i = 0; i < 1+LWM2M_MAX_SERVERS; i++) {
            if (work == (struct k_work *)&connection_update_work[i]) {
                app_server_update(i);
                break;
            }
        }
    }
}


/**@brief Initializes and submits delayed work. */
static void work_init(void)
{
    k_delayed_work_init(&connection_update_work[1], app_connection_update);
    k_delayed_work_init(&connection_update_work[3], app_connection_update);

    k_delayed_work_init(&state_update_work, app_wait_state_update);
}

static void app_lwm2m_observer_process(void)
{
    coap_observer_t * p_observer = NULL;
    while (coap_observe_server_next_get(&p_observer, p_observer, (coap_resource_t *)&m_instance_conn_mon) == 0)
    {
        APPL_LOG("Observer found");
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);
        uint32_t err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                                    &buffer_size,
                                                                    LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH,
                                                                    &m_instance_conn_mon);
        if (err_code)
        {
            LOG_ERR("Could not encode LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH, error code: %lu", err_code);
        }

        m_instance_conn_mon.radio_signal_strength += 1;

        err_code =  lwm2m_notify(buffer,
                                buffer_size,
                                p_observer,
                                COAP_TYPE_CON);
        if (err_code)
        {
            LOG_ERR("Could notify observer, error code: %lu", err_code);
        }
    }
}

#define APP_CUSTOM_APN "VZWADMIN"
void app_apn_connect(void)
{
    int apn_fd;

    apn_fd = nrf_socket(NRF_AF_LTE, NRF_SOCK_MGMT, NRF_PROTO_PDN);
    __ASSERT(apn_fd >= 0, "PDN Socket creation failed.");
    printk("PDN management socket created.\n");
    printk("Connecting to APN %s.\n", APP_CUSTOM_APN);

    // Connect to the APN.
    int err = nrf_connect(apn_fd, APP_CUSTOM_APN, strlen(APP_CUSTOM_APN));
    __ASSERT(err == 0, "APN Connection Failed.");

    send_at_command("AT+CGDCONT?", true);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    printk("\n\nInitializing LTE link, please wait...\n");

    // Initialize Non-volatile Storage.
    app_flash_init();
    app_read_flash_storage();

    if (strcmp(m_device_settings.modem_logging, "2") == 0) {
        modem_trace_enable();
    }

#if CONFIG_DK_LIBRARY
    // Initialize LEDs and Buttons.
    app_buttons_leds_init();
    app_check_buttons_pressed();
#endif

    // Initialize CoAP.
    app_coap_init();

    // Setup LWM2M endpoints.
    app_lwm2m_setup();

    // Turn on SIM to resolve MSISDN.
    lte_lc_init_and_connect();
    read_emei_and_msisdn();
    lte_lc_offline();

    // Create LwM2M factory bootstraped objects.
    app_lwm2m_create_objects();

    // Establish LTE link.
    lte_lc_init_and_connect();

    if (strcmp(m_device_settings.modem_logging, "1") == 0) {
        // 1,0 = disable
        // 1,1 = coredump only
        // 1,2 = generic (and coredump)
        // 1,3 = lwm2m   (and coredump)
        // 1,4 = ip only (and coredump)
        send_at_command("AT%XMODEMTRACE=1,2", false);
        send_at_command("AT%XMODEMTRACE=1,3", false);
        send_at_command("AT%XMODEMTRACE=1,4", false);
    } else if (strlen(m_device_settings.modem_logging) == 64) {
        char at_command[128];
        sprintf(at_command, "AT%%XMODEMTRACE=2,,3,%s", m_device_settings.modem_logging);
        send_at_command(at_command, false);
    }

#if (APP_USE_CONTABO != 1)
    app_apn_connect();
#endif

#if CONFIG_AT_HOST_LIBRARY
    int at_host_err = at_host_init(CONFIG_AT_HOST_UART, CONFIG_AT_HOST_TERMINATION);
    if (at_host_err != 0) {
        LOG_ERR("AT Host not initialized");
    }
#endif

    work_init();

    if (m_server_settings[0].is.bootstrapped)
    {
        m_app_state = APP_STATE_SERVER_CONNECT;
    }
    else
    {
        m_app_state = APP_STATE_BS_CONNECT;
    }
    m_server_instance = 1;

    // Enter main loop
    for (;;)
    {
        if (IS_ENABLED(CONFIG_LOG)) {
            /* if logging is enabled, sleep */
            k_sleep(K_MSEC(10));
        } else {
            /* other, put CPU to idle to save power */
            k_cpu_idle();
        }

        if (tick_count++ % 100 == 0) {
            // Pass a tick to CoAP in order to re-transmit any pending messages.
            ARG_UNUSED(coap_time_tick());
        }

        app_lwm2m_process();

        if (tick_count % 1000 == 0)
        {
            app_lwm2m_observer_process();
        }

#if CONFIG_AT_HOST_LIBRARY
        if (at_host_err == 0) {
            at_host_process();
        }
#endif
    }
}

/**
 * @}
 */
