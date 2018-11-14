/*$$$LICENCE_NORDIC_STANDARD<2015>$$$*/
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#define BUTTONS_AND_LEDS (0)

#if BUTTONS_AND_LEDS
#include "boards.h"
#endif // BUTTONS_AND_LEDS

#include "bsd.h"
#include "nordic_common.h"
#include "sdk_config.h"
#include "app_timer.h"
#include "app_mem_manager.h"
#include "iot_timer.h"
#include "coap_api.h"
#include "coap_option.h"
#include "lwm2m_api.h"
#include "lwm2m_remote.h"
#include "lwm2m_acl.h"
#include "lwm2m_objects_tlv.h"
#include "lwm2m_objects_plain_text.h"

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
#define CLIENT_UUID                     "0a18de70-0ce0-4570-bce9-7f5895db6c70"                /**< UUID of the device. */
#define CLIENT_IMEI_MSISDN              "urn:imei-msisdn:004402990020129-0123456789"          /**< IMEI-MSISDN of the device. */

#if BUTTONS_AND_LEDS
#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK
#define ALL_APP_LED                    (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                        BSP_LED_2_MASK | BSP_LED_3_MASK)                      /**< Define used for simultaneous operation of all application LEDs. */
#endif // BUTTONS_AND_LEDS

#define LED_BLINK_INTERVAL_MS           30000                                                 /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS           50000                                                 /**< Interval between periodic callbacks to CoAP module. */

#define SECURITY_SERVER_URI_SIZE_MAX    64                                                    /**< Max size of server URIs. */

#define BOOTSTRAP_SECURITY_INSTANCE_IDX 0                                                     /**< Index of bootstrap security instance. */
#define SERVER_SECURITY_INSTANCE_IDX    1                                                     /**< Index of server security instance. */
#define APP_RTR_SOLICITATION_DELAY      500                                                   /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                            /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME             128                                                   /**< Max length of filename to copy for the debug error handler. */

#define APP_ENABLE_LOGS                 0                                                     /**< Enable logs in the application. */

#define APP_BOOTSRAP_SEC_TAG            25                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#define APP_BOOTSRAP_SEC_PSK            "d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747" /**< Pre-shared key used for bootstrap server in hex format. */
#define APP_BOOTSRAP_SEC_IDENTITY       "urn:imei-msisdn:004402990020129-0123456789"          /**< Client identity used for bootstrap server. */

#define APP_SERVER_SEC_TAG              26                                                    /**< Tag used to identify security credentials used by the client for bootstrapping. */
#if (DM_SERVER == 1)
#define APP_SERVER_SEC_PSK              "b0373712dc63bfce4be1f37ad8c5c15d3af5f831652af5f46a52c34d39535644" /**< Pre-shared key used for resource server in hex format. */
#else
#define APP_SERVER_SEC_PSK              "77aec743382c61ea43f7af126080146bdf2448b18036b28d2a5bd5006df6fb75" /**< Pre-shared key used for resource server in hex format. */
#endif
#define APP_SERVER_SEC_IDENTITY         "004402990020129"                                     /**< Client identity used for resource server. */

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

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                       /**< App timer instance used to update the IoT timer wall clock. */

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

#define APP_COAP_BUFFER_COUNT_PER_PORT 2                                                      /**< Number of buffers needed per port - one for RX and one for TX */
#define APP_COAP_BUFFER_PER_PORT       (COAP_MESSAGE_DATA_MAX_SIZE * APP_COAP_BUFFER_COUNT_PER_PORT)
#define APP_COAP_MAX_BUFFER_SIZE       (APP_COAP_BUFFER_PER_PORT * COAP_PORT_COUNT)           /**< Maximum memory buffer used for memory allocator for CoAP */

#define APP_STRING_BUFFER_SIZE         64                                                     /**< Maximum string length in LwM2M objects. */
#define APP_STRING_BUFFER_COUNT        10                                                     /**< Number of strings in LwM2M objects. */
#define APP_STRING_BUFFER_MEM_SIZE     (APP_STRING_BUFFER_SIZE * APP_STRING_BUFFER_COUNT)     /**< Memory size needed for strings. */

#define APP_MEM_MAX_BUFFER_SIZE        (APP_STRING_BUFFER_MEM_SIZE + APP_COAP_MAX_BUFFER_SIZE)/**< Maximum memory buffers. */
#define APP_MEM_POOL_COUNT             2                                                      /**< Number of memory pools used. */

// TODO: Fine-tune memory blocks used for LwM2M strings
static uint8_t m_app_coap_data_buffer[APP_MEM_MAX_BUFFER_SIZE];                               /**< Buffer contributed by CoAP for its use. */

/**< Pool submitted to the memory management. */
static const nrf_mem_pool_t m_app_coap_pool[APP_MEM_POOL_COUNT] =
{
    {
        .size  = APP_STRING_BUFFER_SIZE,
        .count = APP_STRING_BUFFER_COUNT
    },
    {
        .size  = COAP_MESSAGE_DATA_MAX_SIZE,
        .count = (APP_COAP_BUFFER_COUNT_PER_PORT * COAP_PORT_COUNT)
    }
};

/**< Configuration used for memory contribution. */
static const nrf_mem_desc_t app_coap_mem_desc =
{
    .mem_type       = NRF_MEM_TYPE_DEFAULT,
    .policy         = NRF_MEM_POLICY_DEFAULT,
    .p_memory       = (uint8_t *)m_app_coap_data_buffer,
    .memory_size    = APP_MEM_MAX_BUFFER_SIZE,
    .pool_list_size = APP_MEM_POOL_COUNT,
    .p_pool_list    = (nrf_mem_pool_t *)m_app_coap_pool
};

#if defined(APP_USE_AF_INET6)

static struct sockaddr_in6 m_bs_server =
{
    .sin_port   = LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT,
    .sin_family = AF_INET6,
    .sin_len    = sizeof(struct sockaddr_in6)
};

static struct sockaddr_in6 m_server =
{
    .sin_port   = LWM2M_SERVER_REMORT_PORT,
    .sin_family = AF_INET6,
    .sin_len    = sizeof(struct sockaddr_in6)
};

#else //APP_USE_AF_INET6

static struct sockaddr_in m_bs_server =
{
    .sin_port        = HTONS(LWM2M_BOOTSTRAP_SERVER_REMOTE_PORT),
    .sin_family      = AF_INET,
    .sin_len         = sizeof(struct sockaddr_in),
    .sin_addr.s_addr = HTONL(0xCF4720E5) // 207.71.32.229
};

static struct sockaddr_in m_server =
{
    .sin_port        = HTONS(LWM2M_SERVER_REMORT_PORT),
    .sin_family      = AF_INET,
    .sin_len         = sizeof(struct sockaddr_in),
#if (DM_SERVER == 1)
    .sin_addr.s_addr = HTONL(0xCF4720E6) // 207.71.32.230 = DM server
#else
    .sin_addr.s_addr = HTONL(0xCF4720E7) // 207.71.32.231 = Repository server
#endif
};

#endif

static const struct sockaddr *   mp_bs_remote_server = (struct sockaddr *)&m_bs_server;       /**< Pointer to remote bootstrap server address to connect to. */
static const struct sockaddr *   mp_remote_server = (struct sockaddr *)&m_server;             /**< Pointer to remote secure server address to connect to. */

static void app_server_update(uint16_t instance_id);


/**@brief Callback function for asserts.
 *
 * This function will be called in case of an assert.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t error)
{
    UNUSED_VARIABLE(error);

    while (true)
    {
        ;
    }
}


/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t error)
{
    UNUSED_VARIABLE(error);

    while (true)
    {
        ;
    }
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by this application.
 */
static void leds_init(void)
{
#if BUTTONS_AND_LEDS
    // Configure LEDs.
    LEDS_CONFIGURE((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));

    // Turn LEDs off.
    LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
#endif // BUTTONS_AND_LEDS
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);
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
static void app_coap_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    // Pass a tick to CoAP in order to re-transmit any pending messages.
    //(void)coap_time_tick();
}


/**@brief Function for updating the wall clock of the IoT Timer module.
 */
static void iot_timer_tick_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
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

    return NRF_SUCCESS;
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
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Save the short_server_id
    err_code = lwm2m_remote_remote_save((struct sockaddr *)p_remote, short_server_id);

    if (err_code != NRF_SUCCESS)
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
    if (err_code != NRF_SUCCESS)
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
            app_nrf_free(mp_link_format_string);
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
#if BUTTONS_AND_LEDS
    LEDS_ON(LED_THREE);
#endif
    (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

    m_app_state = APP_STATE_BOOTSTRAPED;

    return NRF_SUCCESS;
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
        return NRF_SUCCESS;
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

        if (err_code == NRF_ERROR_NOT_FOUND)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return NRF_SUCCESS;
        }

        APP_ERROR_CHECK(err_code);

        (void)lwm2m_respond_with_payload(buffer, buffer_size, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != NRF_SUCCESS)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return NRF_SUCCESS;
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
            return NRF_SUCCESS;
        }

        if (err_code == NRF_SUCCESS)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else if (err_code == NRF_ERROR_NOT_SUPPORTED)
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
                return NRF_SUCCESS;
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
        return NRF_SUCCESS;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return NRF_SUCCESS;
    }

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        err_code = lwm2m_tlv_device_encode(buffer,
                                           &buffer_size,
                                           resource_id,
                                           &m_instance_device);

        if (err_code == NRF_ERROR_NOT_FOUND)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return NRF_SUCCESS;
        }

        APP_ERROR_CHECK(err_code);

        (void)lwm2m_respond_with_payload(buffer, buffer_size, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != NRF_SUCCESS)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return NRF_SUCCESS;
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
            return NRF_SUCCESS;
        }

        if (err_code == NRF_SUCCESS)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else if (err_code == NRF_ERROR_NOT_SUPPORTED)
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
                return NRF_SUCCESS;
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
        return NRF_SUCCESS;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return NRF_SUCCESS;
    }

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        err_code = lwm2m_tlv_connectivity_monitoring_encode(buffer,
                                                            &buffer_size,
                                                            resource_id,
                                                            &m_instance_conn_mon);
        if (err_code == NRF_ERROR_NOT_FOUND)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
            return NRF_SUCCESS;
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

    uint32_t err_code = NRF_SUCCESS;

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
        return NRF_SUCCESS;
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

    return NRF_SUCCESS;
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

    return NRF_SUCCESS;
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
    (void)lwm2m_init(app_nrf_malloc, app_nrf_free);
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
                .sin6_port   = HTONS(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin6_family = AF_INET6,
                .sin6_len    = sizeof(struct sockaddr_in6),
                .sin
            };

        #else // APP_USE_AF_INET6

            const struct sockaddr_in client_addr =
            {
                .sin_port        = HTONS(LWM2M_BOOTSTRAP_LOCAL_CLIENT_PORT),
                .sin_family      = AF_INET,
                .sin_len         =  sizeof(struct sockaddr_in),
                .sin_addr.s_addr = 0
            };

        #endif // APP_USE_AF_INET6

            #define SEC_TAG_COUNT 1

            struct sockaddr * p_localaddr = (struct sockaddr *)&client_addr;
            nrf_sec_tag_t     sec_tag_list[SEC_TAG_COUNT] = {APP_BOOTSRAP_SEC_TAG};

            nrf_sec_config_t setting =
            {
                .role           = 0,    // 0 -> Client role
                .sec_tag_count  = SEC_TAG_COUNT,
                .p_sec_tag_list = &sec_tag_list[0]
            };


            coap_local_t local_port =
            {
                .p_addr    = p_localaddr,
                .p_setting = &setting,
                .protocol  = SPROTO_DTLS1v2
            };

            // NOTE: This method initiates a DTLS handshake and may block for a some seconds.
            err_code = coap_security_setup(&local_port, mp_bs_remote_server);

            if (err_code == NRF_SUCCESS)
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
    if (err_code == NRF_SUCCESS)
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
            .sin6_port   = HTONS(LWM2M_LOCAL_CLIENT_PORT)
            .sin6_family = HTONS(AF_INET6),
            .sin6_len    = sizeof(struct sockaddr_in6);
            .sin
        };

    #else // APP_USE_AF_INET6

        const struct sockaddr_in client_addr =
        {
            .sin_port        = HTONS(LWM2M_LOCAL_CLIENT_PORT),
            .sin_family      = AF_INET,
            .sin_len         =  sizeof(struct sockaddr_in),
            .sin_addr.s_addr = 0
        };

    #endif // APP_USE_AF_INET6

        #define SEC_TAG_COUNT 1

        struct sockaddr * p_localaddr = (struct sockaddr *)&client_addr;
        nrf_sec_tag_t     sec_tag_list[SEC_TAG_COUNT] = {APP_SERVER_SEC_TAG};

        nrf_sec_config_t setting =
        {
            .role           = 0,    // 0 -> Client role
            .sec_tag_count  = SEC_TAG_COUNT,
            .p_sec_tag_list = &sec_tag_list[0]
        };


        coap_local_t local_port =
        {
            .p_addr    = p_localaddr,
            .p_setting = &setting,
            .protocol  = SPROTO_DTLS1v2
        };

        // NOTE: This method initiates a DTLS handshake and may block for some seconds.
        err_code = coap_security_setup(&local_port, mp_remote_server);

        if (err_code == NRF_SUCCESS)
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
    mp_link_format_string = app_nrf_malloc(m_link_format_string_len);

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

        if (err_code == NRF_SUCCESS)
        {
            m_app_state = APP_STATE_SERVER_REGISTERED;
        }
    }
}


static void app_server_update(uint16_t instance_id)
{
    uint32_t err_code;

    // TODO: check instance_id
    UNUSED_PARAMETER(instance_id);

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
            app_bootstrap_connect();
            break;
        }
        case APP_STATE_BS_CONNECTED:
        {
            app_bootstrap();
            break;
        }
        case APP_STATE_BOOTSTRAPED:
        {
            app_server_connect();
            break;
        }
        case APP_STATE_SERVER_CONNECTED:
        {
            app_server_register();
            break;
        }
        case APP_STATE_DISCONNECT:
        {
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
    // Contribute memory needed for CoAP.
    nrf_mem_id_t mem_pid;
    uint32_t err_code = app_nrf_mem_register(&mem_pid, &app_coap_mem_desc);
    APP_ERROR_CHECK(err_code);

    #if defined(APP_USE_AF_INET6)
    struct sockaddr_in6 local_client_addr =
    {
        .sin6_port   = HTONS(COAP_LOCAL_LISTENER_PORT),
        .sin6_family = AF_INET6,
        .sin6_len    = sizeof(struct sockaddr_in6)
    };
    #else // APP_USE_AF_INET6
    struct sockaddr_in local_client_addr =
    {
        .sin_port   = HTONS(COAP_LOCAL_LISTENER_PORT),
        .sin_family = AF_INET,
        .sin_len    = sizeof(struct sockaddr_in)
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

    err_code = coap_init(17, &port_list, app_nrf_malloc, app_nrf_free);
    APP_ERROR_CHECK(err_code);

    mp_coap_transport = local_port_list[0].p_transport;
    UNUSED_VARIABLE(mp_coap_transport);
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
    UNUSED_RETURN_VALUE(snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_BOOTSRAP_SEC_TAG,
                                 IDENTITY_CODE,
                                 APP_BOOTSRAP_SEC_IDENTITY));

    bytes_written = write(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer));
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    UNUSED_RETURN_VALUE(snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_BOOTSRAP_SEC_TAG,
                                 PSK_CODE,
                                 APP_BOOTSRAP_SEC_PSK));

    bytes_written = write(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer));
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);


    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    UNUSED_RETURN_VALUE(snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_SERVER_SEC_TAG,
                                 IDENTITY_CODE,
                                 APP_SERVER_SEC_IDENTITY));

    bytes_written = write(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer));
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    memset (m_at_write_buffer, 0, APP_MAX_AT_WRITE_LENGTH);
    UNUSED_RETURN_VALUE(snprintf(m_at_write_buffer,
                                 APP_MAX_AT_WRITE_LENGTH,
                                 "AT%%CMNG=%d,%d,%d,\"%s\"",
                                 WRITE_OPCODE,
                                 APP_SERVER_SEC_TAG,
                                 PSK_CODE,
                                 APP_SERVER_SEC_PSK));

    bytes_written = write(at_socket_fd, m_at_write_buffer, strlen(m_at_write_buffer));
    APP_ERROR_CHECK_BOOL(bytes_written == strlen(m_at_write_buffer));

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    UNUSED_RETURN_VALUE(close(at_socket_fd));
}


/**@brief Function to configure the modem and create a LTE connection. */
static void app_modem_configure(void)
{
    int at_socket_fd  = -1;
    int bytes_written = 0;
    int bytes_read    = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    APP_ERROR_CHECK_BOOL(at_socket_fd >= 0);

    bytes_written = write(at_socket_fd, "AT+CEREG=2", 10);
    APP_ERROR_CHECK_BOOL(bytes_written == 10);

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    bytes_written = write(at_socket_fd, "AT+CFUN=1", 9);
    APP_ERROR_CHECK_BOOL(bytes_written == 9);

    bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);
    APP_ERROR_CHECK_BOOL(bytes_read >= 2);
    APP_ERROR_CHECK_BOOL(strncmp("OK", m_at_read_buffer, 2) == 0);

    while (true)
    {
        bytes_read = read(at_socket_fd, m_at_read_buffer, APP_MAX_AT_READ_LENGTH);

        if ((strncmp("+CEREG: 1", m_at_read_buffer, 9) == 0) ||
            (strncmp("+CEREG:1", m_at_read_buffer, 8) == 0) ||
            (strncmp("+CEREG: 5", m_at_read_buffer, 9) == 0) ||
            (strncmp("+CEREG:5", m_at_read_buffer, 8) == 0))
        {
            break;
        }
    }
    UNUSED_RETURN_VALUE(close(at_socket_fd));

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
    // Initialize application memory manager.
    UNUSED_RETURN_VALUE(app_nrf_mem_init());

    // Initialize the BSD layer.
    bsd_init();

    // Provision credentials used for the bootstrap server.
    app_provision();

     // Initialize CoAP.
    app_coap_init();

    // Setup LWM2M endpoints.
    lwm2m_setup();

    // Establish LTE link.
    app_modem_configure();

    // Initialize LEDS and Timers.
    leds_init();
    timers_init();
    iot_timer_init();

    // Create LwM2M factory bootstraped objects.
    app_lwm2m_factory_bootstrap();

    // Enter main loop
    for (;;)
    {
        app_lwm2m_process();
    }
}

/**
 * @}
 */
