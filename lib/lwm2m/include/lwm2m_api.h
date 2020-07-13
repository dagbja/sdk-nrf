/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m_api.h
 *
 * @defgroup iot_sdk_lwm2m_api LWM2M Application Programming Interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief Public API of Nordic's LWM2M implementation.
 */
#ifndef LWM2M_API_H__
#define LWM2M_API_H__

#include <stdint.h>
#include <stdbool.h>

#include <coap_api.h>
#include <coap_observe_api.h>
#include <nrf_socket.h>

#include <lwm2m_cfg.h>

#define USE_SHORT_SMS 0

/**@addtogroup LWM2M_opcodes Types
 * @{
 * @brief LWMW2M Bootstrap type definitions.
 */

/**@brief LWM2M time type. */
typedef int32_t lwm2m_time_t;

/**@brief LWM2M string type. */
typedef struct
{
    char   * p_val; /**< Pointer to the value of the string data. */
    uint32_t len;   /**< Length of p_val. */
} lwm2m_string_t;

/**@brief LWM2M opaque type. */
typedef struct
{
    uint8_t * p_val; /**< Pointer to the value of the opaque data. */
    uint32_t  len;   /**< Length of p_val. */
} lwm2m_opaque_t;

/**@bried Data types used in lwm2m_list_t. */
typedef enum
{
    LWM2M_LIST_TYPE_UINT8,
    LWM2M_LIST_TYPE_UINT16,
    LWM2M_LIST_TYPE_INT32,
    LWM2M_LIST_TYPE_STRING
} lwm2m_list_type_t;

/**@brief LWM2M list type. */
typedef struct
{
    lwm2m_list_type_t    type;           /**< Data type used in the list */
    uint16_t           * p_id;           /**< List of resource identifiers for the corresponding value. Must be same size as the `val` array. Set to NULL when enumerating from 0. */
    union {
        uint8_t        * p_uint8;        /**< List of uint8 values */
        uint16_t       * p_uint16;       /**< List of uint16 values */
        int32_t        * p_int32;        /**< List of int32 values */
        lwm2m_string_t * p_string;       /**< List of strings values */
    } val;
    uint32_t             len;            /**< Number of values used in the list */
    uint32_t             max_len;        /**< Maximum number of values in the list */
} lwm2m_list_t;

/**@brief Application notification callback types. */
typedef enum
{
    LWM2M_NOTIFCATION_TYPE_BOOTSTRAP, /**< Notification from a bootstrap request. */
    LWM2M_NOTIFCATION_TYPE_REGISTER,  /**< Notification from a register request. */
    LWM2M_NOTIFCATION_TYPE_UPDATE,    /**< Notification from a update request. */
    LWM2M_NOTIFCATION_TYPE_DEREGISTER /**< Notification from a deregister request. */
} lwm2m_notification_type_t;

/**@brief Enumeration of CoAP option types. */
typedef enum {
	COAP_OPT_TYPE_EMPTY = 0, /**< Empty option type. */
	COAP_OPT_TYPE_UINT,      /**< UINT option type (2 or 4 bytes). */
	COAP_OPT_TYPE_STRING,    /**< String option type. */
	COAP_OPT_TYPE_OPAQUE     /**< Opaque type. */
} coap_option_type_t;

/**@brief Structure to hold a CoAP option, including type. */
typedef struct {
	coap_option_t      coap_opts; /**< Regular option values. */
	coap_option_type_t type;      /**< Option type. */
} coap_option_with_type_t;

/**@brief LWM2M server configuration type. 
 * @note: Option numbers must be in ascending order.
 * @note: Vendor-specific option numbers must be >= 2048.
 */
typedef struct
{
    uint32_t                 lifetime;            /**< Lifetime parameter. */
    lwm2m_string_t           msisdn;              /**< SMS number MSISDN. */
    uint8_t                  lwm2m_version_major; /**< LWM2M major version number. */
    uint8_t                  lwm2m_version_minor; /**< LWM2M minor version number. */
    uint16_t                 short_server_id;     /**< Short server id. */
    lwm2m_string_t           binding;             /**< Binding mode. LWM2M section 5.3.1.1. */
    coap_option_with_type_t* p_options;           /**< Pointer to list of options to include. */
    uint8_t                  num_options;         /**< Number of options to include. */
} lwm2m_server_config_t;

/**@brief LWM2M client identity types. */
typedef enum
{
    LWM2M_CLIENT_ID_TYPE_UUID = 36,
    LWM2M_CLIENT_ID_TYPE_IMEI = 15,
#if USE_SHORT_SMS
    LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN = 42,
#else
    LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN = 44,
#endif
    LWM2M_CLIENT_ID_TYPE_ESN  = 8,
    LWM2M_CLIENT_ID_TYPE_MEID = 14
} lwm2m_client_identity_type_t;

/**@brief LWM2M identity string.
 *
 * @details Using the string representation of UUID/OPS/OS/IMEI/ESN/MEID.
 *
 * @note: OPS- and OS URN are not currently supported.
 */
typedef union
{
    char uuid[LWM2M_CLIENT_ID_TYPE_UUID];
    char imei[LWM2M_CLIENT_ID_TYPE_IMEI];
    char imei_msisdn[LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN];
    char esn[LWM2M_CLIENT_ID_TYPE_ESN];
    char meid[LWM2M_CLIENT_ID_TYPE_MEID];
} lwm2m_identity_string_t;

/**@brief LWM2M client identity structure type. */
typedef struct
{
    uint16_t                     len;
    lwm2m_identity_string_t      value;
    lwm2m_client_identity_type_t type;
} lwm2m_client_identity_t;
/**@} */

/**@addtogroup LWM2M_defines Defines
 * @{
 * @brief LWMW2M operation code and invalid object/instance.
 */

/**
 * @warning The invalid resource and instance are not stated by the lwm2m spec as reserved and will
 *          cause issues if instances or resources with these IDs is added.
 */
#define LWM2M_NAMED_OBJECT            65535 /**< Flag to indicate that the object does not use Integer as object id. */
#define LWM2M_INVALID_RESOURCE        65535 /**< Invalid Resource ID. */
#define LWM2M_INVALID_INSTANCE        65535 /**< Invalid Instance ID. */

 /* Passed to the instance callback as the `resource` parameter
  * when the operation involves an object instance
  */
#define LWM2M_OBJECT_INSTANCE         65535

#define LWM2M_PERMISSION_READ         0x01  /**< Bit mask for LWM2M read permission. */
#define LWM2M_PERMISSION_WRITE        0x02  /**< Bit mask for LWM2M write permission. */
#define LWM2M_PERMISSION_EXECUTE      0x04  /**< Bit mask for LWM2M execute permission. */
#define LWM2M_PERMISSION_DELETE       0x08  /**< Bit mask for LWM2M delete permission. */
#define LWM2M_PERMISSION_CREATE       0x10  /**< Bit mask for LWM2M create permission. */
#define LWM2M_PERMISSION_OBSERVE      0x40  /**< Bit mask for LWM2M observe permission. */

#define LWM2M_OPERATION_CODE_NONE       0x00                        /**< Bit mask for LWM2M no operation. */
#define LWM2M_OPERATION_CODE_READ       LWM2M_PERMISSION_READ       /**< Bit mask for LWM2M read operation. */
#define LWM2M_OPERATION_CODE_WRITE      LWM2M_PERMISSION_WRITE      /**< Bit mask for LWM2M write operation. */
#define LWM2M_OPERATION_CODE_EXECUTE    LWM2M_PERMISSION_EXECUTE    /**< Bit mask for LWM2M execute operation. */
#define LWM2M_OPERATION_CODE_DELETE     LWM2M_PERMISSION_DELETE     /**< Bit mask for LWM2M delete operation. */
#define LWM2M_OPERATION_CODE_CREATE     LWM2M_PERMISSION_CREATE     /**< Bit mask for LWM2M create operation. */
#define LWM2M_OPERATION_CODE_DISCOVER   0x20                        /**< Bit mask for LWM2M discover operation. */
#define LWM2M_OPERATION_CODE_OBSERVE    LWM2M_PERMISSION_OBSERVE    /**< Bit mask for LWM2M observe operation. */
#define LWM2M_OPERATION_CODE_WRITE_ATTR 0x80                        /**< Bit mask for LWM2M write-attribute operation. */

#define LWM2M_URI_PATH_MAX_LEN            4 /**< Maximum supported length of the URI path to identify an LwM2M resource. */
#define LWM2M_MAX_APN_COUNT               3 /**< Maximum supported APNs. */

#define LWM2M_ACL_DEFAULT_SHORT_SERVER_ID    0
#define LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID  65535
/**@} */

/**@cond */
// Forward declare structs.
typedef struct lwm2m_object_t   lwm2m_object_t;
typedef struct lwm2m_instance_t lwm2m_instance_t;
/**@endcond */

/**@brief Signature of function registered by the application
 *        with module to allocate memory for internal module use.
 *
 * @param[in] size Size of memory to be used.
 *
 * @retval A valid memory address on success, else NULL.
 */
typedef void * (*lwm2m_alloc_t) (size_t size);

/**@brief Signature of function registered by the application with
 *        module to free the memory allocated by the module.
 *
 * @param[in] p_memory Address of memory to be freed.
 */
typedef void (*lwm2m_free_t)(void * p_memory);

/**@brief Callback function upon requests on a given LWM2M resource instance.
 *
 * @details Will be called when the request is for an instance Ex. /0/1.
 *
 * If no instance could be located the object callback will be called.
 * The instance_id corresponds to the one in the URI-patch in the CoAP request and may be used to
 * create a new instance. If the value of resource_id is set to LWM2M_INVALID_RESOURCE the callback
 * should treated it as a call to the instance instead of a resource inside of the instance.
 *
 * If a resource has been found p_instance pointer will be set, else it will be NULL.
 *
 * @param[in] p_instance   Pointer to the located resource if it already exists.
 * @param[in] resource_id  Id of the resource requested.
 * @param[in] op_code      Opcode of the request. Values of the opcodes are defined
 *                         in \ref LWM2M_opcodes.
 * @param[in] p_request    Pointer to the CoAP request message.
 *
 * @retval NRF_SUCCESS     Will always return success.
 */
typedef uint32_t (* lwm2m_instance_callback_t)(lwm2m_instance_t * p_instance,
                                               uint16_t           resource_id,
                                               uint8_t            op_code,
                                               coap_message_t   * p_request);

/**@brief Callback function upon request on a given LWM2M object or instance create.
 *
 * @details Will be called when the request is for an object Ex: /0 or /0/1 an instance and the
 *          op_code is CREATE. Depending on the CoAP request code the user might create an instance
 *          or just return the TLV of current instances. If the value of instance_id is set to
 *          LWM2M_INVALID_INSTANCE the callback should treated it as a call to the instance instead
 *          of an instance of the object.
 *
 * @param[in] p_object     Pointer to the located object.
 * @param[in] instance_id  Id of the instance requested.
 * @param[in] op_code      Opcode of the request. Values of the opcodes are defined
 *                         in \ref LWM2M_opcodes.
 * @param[in] p_request    Pointer to the CoAP request message.
 *
 * @retval NRF_SUCCESS     Will always return success.
 */
typedef uint32_t (* lwm2m_object_callback_t)(lwm2m_object_t * p_object,
                                             uint16_t         instance_id,
                                             uint8_t          op_code,
                                             coap_message_t * p_request);

/**@brief Callback function to request a remote server reconnection.
 *
 * @param[in]    p_remote Structure containing address information and port number to the remote server.
 *
 * @return  true if the request has been made, false otherwise.
 */
typedef bool (*lwm2m_request_remote_reconnect_cb_t)(struct nrf_sockaddr *p_remote);

/**@brief LWM2M object prototype structure.
 *
 * @details Each instance will have this structure in the front of its instance structure.
 *          The object is used to have a common way of looking up its object id and callback
 *          structure for each of the inherited. As there is no instance of the objects themselves,
 *          the prototype is used as a meta object in order to have a common set of functions
 *          for all instances of a object kind.
 */
struct lwm2m_object_t
{
    uint16_t                object_id;    /**< Identifies the object. */
    lwm2m_object_callback_t callback;     /**< Called when for request to /0 (object) and /0/1 if instance 1 is not found. */
    char                  * p_alias_name; /**< Alternative name of the resource, used when LWM2M_NAMED_OBJECT is set. */
};

/**@brief LWM2M instance structure.
 *
 * @details Prototype for the instance object, this enables us to search through the instances
 *          without knowing the type.
 */
struct lwm2m_instance_t
{
    uint16_t                  object_id;           /**< Identifies what object this instance belongs to. */
    uint16_t                  instance_id;         /**< Used to identify the instance. */
    uint16_t                  num_resources;       /**< Number of resources MUST equal number of members in the lwm2m instance, sizeof resource_access and sizeof resource_ids. */
    uint8_t                   operations_offset;   /**< Internal use. */
    uint8_t                   resource_ids_offset; /**< Internal use. */
    uint16_t                  expire_time;         /**< Timeout value on instance level to track when to send observable notifications. */
    lwm2m_instance_callback_t callback;            /**< Called when an operation is done on this instance. */
};

/**@brief Callback interface from the enabler interface (bootstrap/register) to the application.
 *
 * @warning This is an interface function. MUST BE IMPLEMENTED BY APPLICATION.
 *
 * @details This interface enables the app to act on different types of notification. When a response from the
 *          bootstrap / register / update / deregister function is received this function will be called to let
 *          the application know the type \ref lwm2m_notification_type_t, remote address of replying server,
 *          the CoAP response code and an error code. The application should check the error code and handle
 *          errors appropriately.
 *
 * @param[in] type      Notification type. The types are defined in \ref lwm2m_notification_type_t.
 * @param[in] p_remote  Remote server that replied.
 * @param[in] coap_code CoAP op code the server responded with.
 * @param[in] err_code  Error code NRF_SUCCESS if successful.
 *
 */
void lwm2m_notification(lwm2m_notification_type_t   type,
                        struct nrf_sockaddr       * p_remote,
                        uint8_t                     coap_code,
                        uint32_t                    err_code);

/**@brief Callback from LwM2M CoAP error handler to the application.
 *
 * @param[in] error_code Error code from CoAP.
 * @param[in] p_message  Message from CoAP.
 *
 * @return true if error is handled, false if error is not handled.
 */
bool lwm2m_coap_error_handler(uint32_t error_code, coap_message_t * p_message);

/**@brief Callback interface from LWM2M core to the application.
 *
 * @warning This is an interface function. MUST BE IMPLEMENTED BY APPLICATION.
 *
 * @details This interface enables the app to act on different types of errors. This function will be called if any
 *          error occurs in the LWM2M instance lookup or ACL checking.
 *
 * @param[in] short_server_id   Short server id of the requesting server. 0 if the server is not registered.
 * @param[in] p_instance        Instance that was resolved for the request. This might be NULL depending on the err_code.
 * @param[in] p_request         Message from COAP.
 * @param[in] err_code          Error code from LWM2M.
 *
 * @retval    NRF_SUCCESS If the error was handled. If the returned value is anything else LWM2M will try to
 *                        do something appropriately for the error.
 *
 */
uint32_t lwm2m_handler_error(uint16_t           short_server_id,
                             lwm2m_instance_t * p_instance,
                             coap_message_t   * p_request,
                             uint32_t           err_code);

/**@brief CoAP Request handler for the root of the object/instance/resource hierarchy.
 *
 * @details The function is called when a request is for the lwm2m root (ie no object instance
 *          or resource).
 *
 * @warning This is an interface function. MUST BE IMPLEMENTED BY APPLICATION.
 *
 * @param[in]  op_code   LWM2M operation code.
 * @param[in]  p_request Pointer to CoAP request message.
 *
 * @retval     NRF_SUCCESS If the handler processed the request successfully.
 */
uint32_t lwm2m_coap_handler_root(uint8_t op_code, coap_message_t * p_request);

/**@brief Initialize LWM2M library.
 *
 * @param[in] alloc_fn Function registered with the module to allocate memory.
 *                     Shall not be NULL.
 * @param[in] free_fn  Function registered with the module to free allocated memory.
 *                     Shall not be NULL.
 *
 * @retval NRF_SUCCESS If initialization was successful.
 */
uint32_t lwm2m_init(lwm2m_alloc_t alloc_fn, lwm2m_free_t free_fn);

/**@brief Send bootstrap request to a remote bootstrap server.
 *
 * @details Sends a bootstrap request with specified ID to the specified remote, calls the
 *          lwm2m_notification with answer from the bootstrap server.
 *
 * @param[in] p_remote    Pointer to the structure holding connection information of the remote
 *                        LWM2M bootstrap server.
 * @param[in] p_id        Pointer to the structure holding the Id of the client.
 * @param[in] transport   Handle to the CoAP Transport Layer.
 * @param[in] p_payload   Pointer to payload. Set to NULL when no payload.
 *
 * @retval NRF_SUCCESS    If bootstrap request to the LWM2M bootstrap server was sent successfully.
 * @retval NRF_ERROR_NULL If one of the parameters was a NULL pointer.
 */
uint32_t lwm2m_bootstrap(struct nrf_sockaddr     * p_remote,
                         lwm2m_client_identity_t * p_id,
                         coap_transport_handle_t   transport,
                         lwm2m_string_t          * p_payload);

/**@brief Register with a remote LWM2M server.
 *
 * @param[in] p_remote             Pointer to the structure holding connection information
 *                                 of the remote LWM2M server.
 * @param[in] p_id                 Pointer to the structure holding the Id of the client.
 * @param[in] p_config             Registration parameters.
 * @param[in] local_port           Port number of the local port to be used to send the
 *                                 register request.
 * @param[in] p_link_format_string Pointer to a link format encoded string to send in the
 *                                 register request.
 * @param[in] link_format_len      Length of the link format string provided.
 *
 * @retval NRF_SUCCESS If registration request to the LWM2M server was sent out successfully.
 */
uint32_t lwm2m_register(struct nrf_sockaddr     * p_remote,
                        lwm2m_client_identity_t * p_id,
                        lwm2m_server_config_t   * p_config,
                        coap_transport_handle_t   transport,
                        uint8_t                 * p_link_format_string,
                        uint16_t                  link_format_len);

/**@brief Update a registration with a remote server.
 *
 * @param[in] p_remote Pointer to the structure holding connection information of the remote
 *                     LWM2M server.
 * @param[in] p_config Registration parameters.
 * @param[in] local_port Port number of the local port to be used to send the update request.
 *
 * @retval NRF_SUCCESS If update request to the LWM2M server was sent out successfully.
 */
uint32_t lwm2m_update(struct nrf_sockaddr     * p_remote,
                      lwm2m_server_config_t   * p_config,
                      coap_transport_handle_t   transport);

/**@brief Deregister from a remote server.
 *
 * @param[in] p_remote Pointer to the structure holding connection information of the remote
 *                     LWM2M server.
 * @param[in] local_port Port number of the local port to be used to send the deregister request.
 *
 * @retval NRF_SUCCESS If deregister request to the LWM2M server was sent out successfully.
 */
uint32_t lwm2m_deregister(struct nrf_sockaddr * p_remote, coap_transport_handle_t transport);

/**@brief Get reference to instance.
 *
 * @param[out] pp_instance  Pointer to structure to instance.
 * @param[in]  object_id    Object identifier of instance.
 * @param[in]  instance_id  Instance identifier of instance.
 *
 * @retval NRF_SUCCESS If instance is found.
 * @retval EINVAL      If instance is not located.
 * @retval EINVAL      If instance is not valid.
 */
uint32_t lwm2m_lookup_instance(lwm2m_instance_t ** pp_instance,
                               uint16_t object_id,
                               uint16_t instance_id);

/**@brief Get reference to object.
 *
 * @param[out] pp_object    Pointer to structure to object.
 * @param[in]  object_id    Object identifier.
 *
 * @retval NRF_SUCCESS If object is found.
 * @retval EINVAL      If object is not located.
 * @retval EINVAL      If object is not valid.
 */
uint32_t lwm2m_lookup_object(lwm2m_object_t  ** pp_object,
                             uint16_t           object_id);

/**
 * @brief Iterate instances
 *
 * @param[in, out]  p_instance  The instance being iterated.
 *                              Pass NULL to begin iterating.
 * @param[in, out]  prog        Progress.
 *
 * @return true     If a new instance was found
 * @return false    When all instances have been iterated
 */
bool lwm2m_instance_next(lwm2m_instance_t **p_instance, size_t *prog);

/**@brief Add an instance to coap_handler in order to match requests to the given instance.
 *
 * @details Add a new LWM2M instance to the coap_handler. The application MUST initialize
 *          and allocate the additional data in the struct.
 *
 * @param[in]  p_instance  Pointer to the instance to add.
 *
 * @retval     NRF_SUCCESS      If registration was successful.
 * @retval     NRF_ERROR_NO_MEM If the module was not able to add the instance. Verify that
 *                              the LWM2M_COAP_HANDLER_MAX_INSTANCES setting in sdk_config.h
 *                              has a correct value.
 */
uint32_t lwm2m_coap_handler_instance_add(lwm2m_instance_t * p_instance);

/**@brief Delete an instance from coap_handler in order to stop matching requests to the given
 *        instance.
 *
 * @param[in]  p_instance Pointer to the instance to delete.
 *
 * @retval     NRF_SUCCESS         If deregister was a success.
 * @retval     NRF_ERROR_NOT_FOUND If the given instance was not located.
 */
uint32_t lwm2m_coap_handler_instance_delete(lwm2m_instance_t * p_instance);

/**@brief Add an object to coap_handler in order to match requests to the given object.
 *
 * @details Add a new LWM2M object to the coap_handler. The application MUST initialize
 *          and allocate the additional data in the struct.
 *
 * @param[in]  p_object  Pointer to the object to add.
 *
 * @retval     NRF_SUCCESS      If registration was successful.
 * @retval     NRF_ERROR_NO_MEM If the module was not able to add the object. Verify that
 *                              the LWM2M_COAP_HANDLER_MAX_OBJECTS setting in sdk_config.h
 *                              has a correct value.
 */
uint32_t lwm2m_coap_handler_object_add(lwm2m_object_t * p_object);

/**@brief Delete an object from coap_handler in order to stop matching requests to the given
 *        object.
 *
 * @param[in]  p_object Pointer to the object to delete.
 *
 * @retval     NRF_SUCCESS         If deregister was a success.
 * @retval     NRF_ERROR_NOT_FOUND If the given object was not located.
 */
uint32_t lwm2m_coap_handler_object_delete(lwm2m_object_t * p_object);

/**@brief Generate link format string based on registered objects and instances.
 *
 * @note For generation of links to work properly it is required that objects is added
 *       before instances.
 *
 * @param[in]    object_id       Identifies a specific object, or LWM2M_INVALID_INSTANCE for all.
 * @param[in]    short_server_id Short server id of the requesting server.
 * @param[inout] p_buffer        Pointer to a buffer to fill with link format encoded string. If
 *                               a NULL pointer is provided the function will dry-run the function
 *                               in order to calculate how much memory that is needed for the link
 *                               format string.
 * @param[inout] p_buffer_len    As input used to indicate the length of the buffer. It will return the
 *                               used amount of buffer length by reference in response. If NULL pointer
 *                               is provided for p_buffer, the value by reference output will be the number
 *                               of bytes needed to generate the link format string.
 *
 * @retval        NRF_SUCCESS      If generation of link format string was successful.
 * @retval        NRF_ERROR_NO_MEM If the provided memory was not large enough.
 */
uint32_t lwm2m_coap_handler_gen_link_format(uint16_t   object_id,
                                            uint16_t   short_server_id,
                                            uint8_t  * p_buffer,
                                            uint16_t * p_buffer_len);

/**@brief Generate link format string based on Object.
 *
 * @param[in]    object_id       Identifies the object.
 * @param[in]    short_server_id Short server id of the requesting server.
 * @param[inout] p_buffer        Pointer to a buffer to fill with link format encoded string.
 * @param[inout] p_buffer_len    As input used to indicate length of the buffer. It will return the
 *                               used amout of buffer length.
 *
 * @retval NRF_SUCCESS If generation of link format string was successful.
 */
uint32_t lwm2m_coap_handler_gen_object_link(uint16_t   object_id,
                                            uint16_t   short_server_id,
                                            uint8_t  * p_buffer,
                                            uint32_t * p_buffer_len);

/**@brief Generate link format string based on Object Instance.
 *
 * @param[in]    p_instance   Pointer to structure to instance.
 * @param[inout] p_buffer     Pointer to a buffer to fill with link format encoded string.
 * @param[inout] p_buffer_len As input used to indicate length of the buffer. It will return the
 *                            used amout of buffer length.
 *
 * @retval NRF_SUCCESS If generation of link format string was successful.
 */
uint32_t lwm2m_coap_handler_gen_instance_link(lwm2m_instance_t * p_instance,
                                              uint16_t           short_server_id,
                                              uint8_t          * p_buffer,
                                              uint32_t         * p_buffer_len);

/**@brief Generate link format string based on Notification Attributes.
 *
 * @param[in]    p_path           URI path that identifies the observable item.
 * @param[in]    path_len         Length of the URI path that identifies the observable item.
 * @param[in]    short_server_id  Short server id of the requesting server.
 * @param[inout] p_buffer_len     As input used to indicate length of the buffer. It will return the
 *                                used amout of buffer length.
 * @param[in]    inherit          Flag to indicate whether the inherited R-Attributes ought to be
 *                                included.
 *
 * @retval NRF_SUCCESS If generation of link format string was successful.
 */
uint32_t lwm2m_coap_handler_gen_attr_link(uint16_t const * p_path,
                                          uint16_t         path_len,
                                          uint16_t         short_server_id,
                                          uint8_t        * p_buffer,
                                          uint32_t       * p_buffer_len,
                                          bool             inherit);

/**@brief Send CoAP 2.05 Content response with the payload provided.
 *
 * @param[in] p_payload    Pointer to the payload to send. Must not be NULL.
 * @param[in] payload_len  Size of the payload.
 * @param[in] content_type CoAP content type.
 * @param[in] p_request    Original CoAP request. Must not be NULL.
 *
 * @retval NRF_SUCCESS If the response was sent out successfully.
 */
uint32_t lwm2m_respond_with_payload(uint8_t             * p_payload,
                                    uint16_t              payload_len,
                                    coap_content_type_t   content_type,
                                    coap_message_t      * p_request);

/**
 * @brief Prepare a CoAP response to an incoming CoAP request.
 *
 * @param pp_rsp The response message.
 * @param p_req  The request message.
 * @return uint32_t 0 on success, an error code otherwise.
 */
uint32_t lwm2m_coap_rsp_new(coap_message_t **pp_rsp, coap_message_t *p_req);

/**
 * @brief Send a CoAP response message with a given response code.
 *
 * @param p_rsp The response message.
 * @param code  The response code to use.
 * @return uint32_t 0 on success, an error code otherwise.
 */
uint32_t lwm2m_coap_rsp_send_with_code(coap_message_t *p_rsp, coap_msg_code_t code);

/**
 * @brief Send a CoAP response message.
 *
 * @param p_rsp The response message to send.
 * @return uint32_t 0 on success, an error code otherwise.
 */
uint32_t lwm2m_coap_rsp_send(coap_message_t *p_rsp);

/**@brief Send CoAP response with a given CoAP message code.
 *
 * @param  [in] code      CoAP response code to send.
 * @param  [in] p_request Original CoAP request. Must not be NULL.
 *
 * @retval NRF_SUCCESS If the response was sent out successfully.
 */
uint32_t lwm2m_respond_with_code(coap_msg_code_t code, coap_message_t * p_request);

/**@brief Send CoAP response with a Bootstrap DISCOVER link-format.
 *
 * @param[in] object_id  Identifies the object.
 * @param[in] p_request  Original CoAP request. Must not be NULL.
 *
 * @retval NRF_SUCCESS If the response was sent out successfully.
 */
uint32_t lwm2m_respond_with_bs_discover_link(uint16_t object_id, coap_message_t * p_request);

/**@brief Send CoAP response with a Object link-format.
 *
 * @param[in] object_id  Identifies the object.
 * @param[in] p_request  Original CoAP request. Must not be NULL.
 *
 * @retval NRF_SUCCESS If the response was sent out successfully.
 */
uint32_t lwm2m_respond_with_object_link(uint16_t object_id, coap_message_t * p_request);

/**@brief Send CoAP response with a Object Instance link-format.
 *
 * @param[in] p_instance  Pointer to structure to instance.
 * @param[in] resource_id Resource ID.
 * @param[in] p_request   Original CoAP request. Must not be NULL.
 *
 * @retval NRF_SUCCESS If the response was sent out successfully.
 */
uint32_t lwm2m_respond_with_instance_link(lwm2m_instance_t * p_instance,
                                          uint16_t           resource_id,
                                          coap_message_t   * p_request);

/**@brief Register an observer on the item specified by the URI path.
 *
 * @param[in]    p_path      URI path that identifies the item to be observed.
 * @param[in]    path_len    Length of the URI path that identifies the item
 *                           to be observed.
 * @param[in]    p_request   Original CoAP request. Must not be NULL.
 * @param[inout] pp_response CoAP message that will be populated with the
 *                           required parameters to acknowledge the observation
 *                           request. Must not be NULL.
 *
 * @return 0  If the observer has been registered successfully or error code on
 *            failure.
 */
uint32_t lwm2m_observe_register(const uint16_t   * p_path,
                                uint8_t            path_len,
                                coap_message_t   * p_request,
                                coap_message_t  ** pp_response);

/**@brief Unregister observer if found.
 *
 * @param p_remote           Remote address of the observing server.
 * @param p_observable       Pointer to the item of interest registered.
 *
 * @retval 0      If the observer has been unregistered successfully.
 * @retval ENOMEM If the observer could not be found in the list.
 * @retval EINVAL If one of the parameters is a NULL pointer.
 */
uint32_t lwm2m_observe_unregister(struct nrf_sockaddr * p_remote,
                                  const void          * p_observable);

uint32_t lwm2m_notify(uint8_t         * p_payload,
                      uint16_t          payload_len,
                      coap_observer_t * p_observer,
                      coap_msg_type_t   type);

/**@brief Set the callback function to request remote server reconnection.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_request_remote_reconnect_cb_set(lwm2m_request_remote_reconnect_cb_t callback);

/**@brief Determine whether the given LWM2M resource is being observed by the given server.
 *
 * @param[in] short_server_id  Short server ID of the potential observer.
 * @param[in] p_observable     Observable item.
 *
 * @return  true if the resource has a registered observer, false otherwise.
 */
bool lwm2m_is_observed(uint16_t short_server_id, const void *p_observable);

/**@brief Send a CoAP message to a given remote.
 *
 * @param[in] coap_message_t   Message to be sent.
 * @param[in] p_remote         Remote server to which the message will be sent.
 * @param[in] p_payload        Message payload to be included. Can be NULL if an
 *                             empty message is to be sent.
 * @param[in] payload_len      Length of the message payload.
 *
 * @return 0  If the message has been sent successfully or an error code on failure.
 */
uint32_t lwm2m_coap_message_send_to_remote(coap_message_t      * p_message,
                                           struct nrf_sockaddr * p_remote,
                                           uint8_t             * p_payload,
                                           uint16_t              payload_len);

/**@brief Retrieve the configured value of the time interval for sending confirmable notifications.
 *
 * @return  Time interval in miliseconds.
 */
int64_t lwm2m_coap_con_interval_get(void);

/**@brief Set the time interval for sending confirmable notifications.
 *
 * @note  If the interval is not set by the application, the value indicated by
 *        CONFIG_NRF_COAP_CON_NOTIFICATION_INTERVAL will be used.
 *
 * @param[in] con_interval     Time interval in seconds.
 */
void lwm2m_coap_con_interval_set(int64_t con_interval);

/**@brief Get an integer from a lwm2m_list_t.
 *
 * @param[in] p_list List to fetch integer from.
 * @param[in] idx    Index of integer in list.
 *
 * @return Integer value at the given index.
 */
int32_t lwm2m_list_integer_get(lwm2m_list_t * p_list, uint32_t idx);

/**@brief Set an integer in a lwm2m_list_t.
 *
 * @param[in] p_list List to set integer in.
 * @param[in] idx    Index of integer in list.
 * @param[in] value  Integer value to set.
 *
 * @retval  0         The set operation is successful.
 * @retval  EINVAL    Illegal list definition.
 * @retval  EMSGSIZE  Index is out of bounds.
 */
uint32_t lwm2m_list_integer_set(lwm2m_list_t * p_list, uint32_t idx, int32_t value);

/**@brief Append an integer to a lwm2m_list_t.
 *
 * @param[in] p_list List to set integer in.
 * @param[in] value  Integer value to append to list.
 *
 * @retval  0         The append operation is successful.
 * @retval  EINVAL    Illegal list definition.
 * @retval  EMSGSIZE  List is full.
 */
uint32_t lwm2m_list_integer_append(lwm2m_list_t * p_list, int32_t value);

/**@brief Get a string from a lwm2m_list_t.
 *
 * @param[in] p_list List to fetch string from.
 * @param[in] idx    Index of string in list.
 *
 * @return Pointer to lwm2m_string_t at the given index or NULL if invalid parameters.
 */
lwm2m_string_t * lwm2m_list_string_get(lwm2m_list_t * p_list, uint32_t idx);

/**@brief Set a string in a lwm2m_list_t.
 *
 * @param[in] p_list    List to set string in.
 * @param[in] idx       Index of string in list.
 * @param[in] p_value   Pointer to string.
 * @param[in] value_len Length of string
 *
 * @retval  0         The set operation is successful.
 * @retval  EINVAL    Illegal list definition.
 * @retval  EMSGSIZE  Index is out of bounds.
 */
uint32_t lwm2m_list_string_set(lwm2m_list_t * p_list, uint32_t idx, const uint8_t * p_value, uint16_t value_len);

/**@brief Append a string to a lwm2m_list_t.
 *
 * @param[in] p_list    List to set string in.
 * @param[in] p_value   Pointer to string.
 * @param[in] value_len Length of string
 *
 * @retval  0         The append operation is successful.
 * @retval  EINVAL    Illegal list definition.
 * @retval  EMSGSIZE  List is full.
 */
uint32_t lwm2m_list_string_append(lwm2m_list_t * p_list, uint8_t * p_value, uint16_t value_len);

/**@brief Convert a URI path into a string.
 *
 * @param[in] p_path   URI path to be converted.
 * @param[in] path_len Length of the URI path to be converted.
 *
 * @return The URI path in string form, or NULL if p_path is NULL.
 */
const char * lwm2m_path_to_string(const uint16_t *p_path, uint8_t path_len);

/**@brief Configure the Access Control context of the LwM2M client.
 *
 * @param[in] enable_status Flag to enable or disable Access Control.
 */
void lwm2m_ctx_access_control_enable_status_set(bool enable_status);

/**@brief Retrieve the current Access Control context of the LwM2M client.
 *
 * @retval true if Access Control is enabled, false otherwise.
 */
bool lwm2m_ctx_access_control_enable_status_get(void);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_API_H__

/** @} */