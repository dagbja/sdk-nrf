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

/**@brief LWM2M server configuration type. */
typedef struct
{
    uint32_t       lifetime;            /**< Lifetime parameter. */
    lwm2m_string_t msisdn;              /**< SMS number MSISDN. */
    uint8_t        lwm2m_version_major; /**< LWM2M major version number. */
    uint8_t        lwm2m_version_minor; /**< LWM2M minor version number. */
    uint16_t       short_server_id;     /**< Short server id. */
    lwm2m_string_t binding;             /**< Binding mode. LWM2M section 5.3.1.1. */
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
 * @brief LWMW2M operation code, invalid object/instance and notification attribute definitions.
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

#define LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD        0 /**< p_min notification attribute type. */
#define LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD        1 /**< p_max notification attribute type. */
#define LWM2M_ATTRIBUTE_TYPE_GREATER_THAN      2 /**< gt notification attribute type. */
#define LWM2M_ATTRIBUTE_TYPE_LESS_THAN         3 /**< lt notification attribute type. */
#define LWM2M_ATTRIBUTE_TYPE_STEP              4 /**< st notification attribute type. */
#define LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE         5 /**< Number of supported notification attribute types. */
#define LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES 30 /**< Number of supported observable items which may have assigned notification attributes. */

#define LWM2M_OBSERVABLE_TYPE_INT         0x01 /**< Observable structure of integer type. */
#define LWM2M_OBSERVABLE_TYPE_STR         0x02 /**< Observable structure of string type. */
#define LWM2M_OBSERVABLE_TYPE_LIST        0x04 /**< Observable structure of lwm2m_list_t type. */
#define LWM2M_OBSERVABLE_TYPE_NO_CHECK    0x0E /**< Observable structure of a noncomparable data type. */

#define LWM2M_URI_PATH_MAX_LEN            4 /**< Maximum supported length of the URI path to identify an LwM2M resource. */
#define LWM2M_MAX_APN_COUNT               2 /**< Maximum supported APNs. */
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

/**@brief Callback function to store observer information in non-volatile memory
 *
 * @defails This function will be used by the observer restore to store observer
 *          information in non-volatile memory.
 *
 * @param[in] sid  Unique ID that identifies this storage unit.
 * @param[in] data Pointer to data to be stored.
 * @param[in] size The size of the data to be stored.
 *
 * @retval    Non-zero value will tell the observer restore feature that the store failed.
 */
typedef int (*lwm2m_store_observer_cb_t)(uint32_t sid, void * data, size_t size);

/**@brief Callback function to load observer information from non-volatile memory.
 *
 * @details This function will be used by the observer restore to load observer
 *          information in non-volatile memory.
 *
 * @param[in] sid  Unique ID that identifies a storage unit previously stored.
 * @param[in] data Pointer to a buffer that the loaded data can be stored in.
 * @param[in] size The size of the data buffer.
 *
 * @retval    Non-zero value will tell the observer restore feature that the load failed.
 */
typedef int (*lwm2m_load_observer_cb_t)(uint32_t sid, void * data, size_t size);

/**@brief Callback function to delete observer information in non-volatile memory.
 *
 * @details This function will be used by the observer restore to delete observer
 *          information in non-volatile memory previously stored.
 *
 * @param[in] sid  Unique ID that identifies a storage unit previously stored.
 *
 * @retval    Non-zero value will tell the observer restore feature that the delete failed.
 */
typedef int (*lwm2m_del_observer_cb_t)(uint32_t sid);

/**@brief Callback function to store notification attributes in non-volatile memory.
 *
 * @param[in] sid  Unique ID that identifies this storage unit.
 * @param[in] data Pointer to data to be stored.
 * @param[in] size The size of the data to be stored.
 *
 * @retval    Non-zero value will tell the notification attribute restore feature
 *            that the store failed.
 */
typedef int (*lwm2m_store_notif_attr_cb_t)(uint32_t sid, void * p_data, size_t size);

/**@brief Callback function to load notification attributes from non-volatile memory.
 *
 * @param[in] sid  Unique ID that identifies a storage unit previously stored.
 * @param[in] data Pointer to a buffer that the loaded data can be stored in.
 * @param[in] size The size of the data buffer.
 *
 * @retval    Non-zero value will tell the notification attribute restore feature
 *            that the load failed.
 */
typedef int (*lwm2m_load_notif_attr_cb_t)(uint32_t sid, void * p_data, size_t size);

/**@brief Callback function to delete notification attributes in non-volatile memory.
 *
 * @param[in] sid  Unique ID that identifies a storage unit previously stored.
 *
 * @retval    Non-zero value will tell the notification attribute restore feature
 *            that the delete failed.
 */
typedef int (*lwm2m_del_notif_attr_cb_t)(uint32_t sid);

/**@brief Callback function to set the default notification attribute values.
 *
 * @details Will be called when a new notification attribute is initialized. LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD
 *          and LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD are always initialized upon observer registration.
 *
 * @param[in]    type            Notification attribute type whose default value is to be retrieved.
 * @param[inout] p_value         Pointer to the memory where the default value of the attribute is to be stored.
 * @param[in]    p_remote_server Structure containing address information and port number to the remote server
 *                               that made the OBSERVE request.
 */
typedef void (*lwm2m_notif_attr_default_cb_t)(uint8_t type, void *p_value, struct nrf_sockaddr *p_remote_server);

/**@brief Callback function to retrieve a pointer to the value of the observable item.
 *
 * @param[in]    p_path     URI path that identifies the observable item.
 * @oaram[in]    path_len   Length of the URI path that identifies the observable item.
 * @param[inout] p_type     Pointer to the memory where the data type of the observable item is stored.
 *
 * @return  Pointer to the observable item data.
 */
typedef const void * (*lwm2m_observable_reference_get_cb_t)(const uint16_t *p_path, uint8_t path_len, uint8_t *p_type);

/**@brief Callback function to retrieve the uptime in milliseconds (ms).
 *
 * @return  Uptime in milliseconds (ms).
 */
typedef int64_t (*lwm2m_uptime_get_cb_t)(void);

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


/**@brief LWM2M access control list structure
 *
 * @details Used to represent the access to one instance. One instance can only have one owner, the owner always have full access rights.
 *          The other servers can have no access or more. This only applies on the instance level, resources are handled by static rights.
 *
 */
typedef struct
{
    uint16_t access[1+LWM2M_MAX_SERVERS];      /**< ACL array. */
    uint16_t server[1+LWM2M_MAX_SERVERS];      /**< Short server id to ACL array index. */
    uint16_t id;                               /**< The id of this ACL instance, has to be unique for each instance. */
    uint16_t owner;                            /**< Owner of this ACL entry. Short server id */
} lwm2m_instance_acl_t;

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
    lwm2m_instance_acl_t      acl;                 /**< ACL entry. */
};

/**@brief LWM2M notification attribute structure. */
typedef struct
{
    union
    {
        int32_t i;                  /**< Used for p_min and p_max notification attributes. */
        float   f;                  /**< Used for gt, lt and st notification attributes. */
    } value;                        /**< Value assigned to the notification attribute. */
    int8_t assignment_level;        /**< Notification attribute assignment level. */
} lwm2m_notif_attribute_t;

/**@brief LWM2M observable metadata structure.
 *
 * @details Structure used to identify the items that are being observed, as well as their assigned notification attributes.
 */
typedef struct
{
    uint16_t                 path[LWM2M_URI_PATH_MAX_LEN];               /**< URI path of the observable structure. */
    uint8_t                  path_len;                                   /**< Length of the URI path of the observable structure. */
    lwm2m_notif_attribute_t  attributes[LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE]; /**< Notification attributes of this observable. */
    lwm2m_time_t             last_notification;                          /**< Time elapsed from the last notification sent. */
    int64_t                  con_notification;                           /**< Last time the notification has been sent as a confirmable message. */
    const void               *observable;                                /**< Pointer to the observable structure. */
    int32_t                  prev_value;                                 /**< The value of the observable structure that was reported in the last notification. */
    uint8_t                  type;                                       /**< Bitcode that identifies the data type of the observable. */
    uint8_t                  flags;                                      /**< Flags indicating whether the conditions defined by the attributes have been fulfilled. */
    uint16_t                 ssid;                                       /**< Short ID of the server that performed an OBSERVE or a WRITE-ATTRIBUTE request on the observable. */
    uint8_t                  changed;                                    /**< Flag indicating whether the value of the observable has changed since the last notification has been sent. */
} lwm2m_observable_metadata_t;

/**@brief Callback interface from the enabler interface (bootstrap/register) to the application.
 *
 * @warning This is an interface function. MUST BE IMPLEMENTED BY APPLICATION.
 *
 * @details This interface enables the app to act on different types of notification. When a response from the
 *          boostrap / register / update / deregister function is received this function will be called to let
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
 *
 * @retval NRF_SUCCESS    If bootstrap request to the LWM2M bootstrap server was sent successfully.
 * @retval NRF_ERROR_NULL If one of the parameters was a NULL pointer.
 */
uint32_t lwm2m_bootstrap(struct nrf_sockaddr     * p_remote,
                         lwm2m_client_identity_t * p_id,
                         coap_transport_handle_t   transport);

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
 *
 * @retval NRF_SUCCESS If generation of link format string was successful.
 */
uint32_t lwm2m_coap_handler_gen_attr_link(uint16_t const * p_path,
                                          uint16_t         path_len,
                                          uint16_t         short_server_id,
                                          uint8_t        * p_buffer,
                                          uint32_t       * p_buffer_len);

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

/**@brief Store information about the observer so it can be restored later.
 *
 * @param p_observer Pointer to an observer object to store.
 * @param p_path     URI path that identifies the item of interest of the
 *                   observer to be stored.
 * @param path_len   Length of the URI path of the item of interest of the
 *                   observer to be stored.
 *
 * @retval 0      If observer information has been stored successfully.
 * @retval ENOMEM If not able to find a free slot to store the observer data.
 * @retval EIO    If storage interface returned error on deletion operation.
 */
uint32_t lwm2m_observer_storage_store(coap_observer_t * p_observer,
                                      const uint16_t  * p_path,
                                      uint8_t           path_len);

/**@brief Store notification attributes of a given observable so that they
 *        can be restored after a power cycle.
 *
 * @param p_observer[in] Pointer to an observable metadata structure to store.
 *
 * @retval 0      If observable metadata structure has been stored successfully.
 * @retval ENOMEM If not able to find a free slot to store the observable
 *                metadata.
 * @retval EIO    If storage interface returned error on store operation.
 */
uint32_t lwm2m_notif_attr_storage_store(const lwm2m_observable_metadata_t * p_metadata);

/**@brief Restore observers for a specified server.
 *
 * @details This function will read the session data and restore any
 *          observer for a specified server over the specified transport.
 *
 * @param short_server_id A unique identifier for the server owning the
 *                        observers that will be restored.
 * @param transport       The handle for the transport to be used for this
 *                        server.
 *
 * @retval         Number of observers restored.
 */
uint32_t lwm2m_observer_storage_restore(uint16_t                  short_server_id,
                                        coap_transport_handle_t   transport);

/**@brief Restore any notification attributes from non-volatile memory.
 *
 * @details This function will reinitialize any notification attributes
 *          that have been modified by a server before the power cycle.
 *
 *  @param[in] short_server_id  A unique identifier for the server owning the
 *                              notification attributes that will be restored.
 *
 * @retval  0      If the notification attributes have been reinitialized
 *                 successfully, if any.
 * @retval  EINVAL If a callback function for loading notification attributes
 *                 from non-volatile storage has not been set.
 */
uint32_t lwm2m_notif_attr_storage_restore(uint16_t short_server_id);

/**@brief Delete an observer from storage.
 *
 * @details This function will delete the stored information about this observer.
 *
 * @param p_observer A pointer to an observer object.
 *
 * @retval 0      If delete process was successful.
 * @retval ENOMEM If unable to allocate memory to store the observer data.
 * @retval EIO    If storage interface returned error on deletion operation.
 *
 */
uint32_t lwm2m_observer_storage_delete(coap_observer_t * p_observer);

/**@brief Delete all the observers from storage.
 *
 * @details This function will delete the stored information regarding all the
 *          observers from the non-volatile storage. It should be called upon
 *          a factory reset.
 */
void lwm2m_observer_storage_delete_all(void);

/**@brief Delete notification attributes of a given observable from storage.
 *
 * @param[in] p_observer A pointer to an observable metadata structure.
 *
 * @retval 0      If delete process was successful.
 * @retval ENOMEM If unable to allocate memory to store the storage entry.
 * @retval EIO    If storage interface returned error on deletion operation.
 *
 */
uint32_t lwm2m_notif_attr_storage_delete(const lwm2m_observable_metadata_t * p_metadata);

/**@brief Update the storage of the notification attributes assigned to the given
 *        observable to reflect their current state.
 *
 * @details This function should be called whenever an observation is cancelled on
 *          a given observable.
 *
 * @param[in] p_path    URI path that identifies the observable item.
 * @param[in] path_len  Length of the URI path that identifies the observable item.
 * @param[in] p_remote  Structure containing address information and port number to the remote observer.
 */
void lwm2m_notif_attr_storage_update(const uint16_t *p_path, uint16_t path_len, struct nrf_sockaddr *p_remote);

/**@brief Delete all the stored notification attributes from storage.
 *
 * @details This function will delete all the notification attributes that have
 *          been stored in the non-volatile storage so far. Should be called
 *          upon any factory reset execution.
 */
void lwm2m_notif_attr_storage_delete_all(void);

/**@brief Register functions for storing, loading and deleting observer
 *        information in non-volatile storage.
 *
 * @details This function is used to register functions to store, load and
 *          delete data on the non-volatile storage backend.
 *
 * @retval 0      If callback functions were registered.
 * @retval EINVAL If any of the callback functions pointers are NULL pointers.
 *
 */
uint32_t lwm2m_observer_storage_set_callbacks(lwm2m_store_observer_cb_t store_cb,
                                              lwm2m_load_observer_cb_t load_cb,
                                              lwm2m_del_observer_cb_t del_cb);

/**@brief Register functions for storing, loading and deleting notification
 *        attributes in non-volatile storage.
 *
 * @retval 0      If callback functions were registered.
 * @retval EINVAL If any of the callback functions pointers are NULL pointers.
 *
 */
uint32_t lwm2m_notif_attr_storage_set_callbacks(lwm2m_store_notif_attr_cb_t store_cb,
                                                lwm2m_load_notif_attr_cb_t load_cb,
                                                lwm2m_del_notif_attr_cb_t del_cb);

/**@brief Initialize a new observable metadata structure.
 *
 * @details Will be called whenever an OBSERVE or a WRITE-ATTRIBUTE operation is
 *          requested on an item that was previously not being observed.
 *
 * @param[in] p_remote    Structure containing address information and port number
 *                        to the remote server that made the request.
 * @param[in] p_path      URI path that identifies the observable item.
 * @param[in] path_len    Length of the URI path that identifies the observable item.
 *
 * @return  A positive value indicating the entry number of the observable in the table,
 *          or a negative value indicating failure and the error code.
 */
int lwm2m_observable_metadata_init(struct nrf_sockaddr *p_remote,
                                   const uint16_t *p_path,
                                   uint8_t path_len);

/**@brief Iterate the registered observers and send notifications if the conditions
 *        established by the corresponding notification attributes are fulfilled.
 *
 * @details This function will iterate the initialized observables and check whether
 *          the notification conditions (determined by the notification attributes)
 *          have been fulfilled since the last notification sent, in which case the
 *          observer will be notified.
 *
 * @param[in] bool      If true, notify the observers regardless of whether the notification
 *                      conditions have been fulfilled, as the LWM2M client has reestablished
 *                      the connection with the server(s).
 */
void lwm2m_observer_process(bool reconnect);

/**@brief Set the callback function to set the default notification attribute values.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_notif_attr_default_cb_set(lwm2m_notif_attr_default_cb_t callback);

/**@brief Set the callback function to retrieve a pointer to the value of the observable item.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_observable_reference_get_cb_set(lwm2m_observable_reference_get_cb_t callback);

/**@brief Set the callback function to retrieve the uptime in milliseconds and initialize
 *        the reference timer that will be used to handle the information reporting interface.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_observable_uptime_cb_initialize(lwm2m_uptime_get_cb_t callback);

/**@brief Set the callback function to request remote server reconnection.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_request_remote_reconnect_cb_set(lwm2m_request_remote_reconnect_cb_t callback);

/**@brief Handler function for incoming write-attribute requests from the LWM2M server.
 *
 * @param[in] p_path     URI path specified in the write-attributes request CoAP message.
 * @param[in] path_len   Length of the URI path specified in the write-attributes request CoAP message.
 * @param[in] p_request  The write-attributes CoAP request message.
 *
 * @return 0  If the notification attributes have been updated successfully or an error code on
 *            failure.
 */
int lwm2m_write_attribute_handler(const uint16_t *p_path, uint8_t path_len, const coap_message_t *p_request);

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

/**@brief Retrieve a pointer to an observable LwM2M item identified by its URI path.
 *
 * @param[in] p_path      URI path that identifies the observable item.
 * @param[in] path_len    Length of the URI path that identifies the observable item.
 *
 * @return  A valid pointer to the observable item if found, or NULL if the item has
 *          not been found or is not observable.
 */
const void * lwm2m_observable_reference_get(const uint16_t *p_path, uint8_t path_len);

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

/**@brief Determine whether the next notification ought to be a confirmable message.
 *
 * @details The function will determine whether the next notification regarding the
 *          given observable should be sent as a confirmable CoAP message, according
 *          to the configured interval.
 *
 * @param[in] p_observable     Item of interest of the observer.
 * @param[in] ssid             Short ID of the server that requested the observation.
 *
 * @return  true if the notification is to be sent as a CON message.
 */
bool lwm2m_observer_notification_is_con(const void *p_observable, uint16_t ssid);

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

/**@brief Report that the value of an observable resource has changed.
 *
 * @note The function will set the "changed" flag of the observable resource, if its
 *       observable metadata has been initialized (either by requesting an observation
 *       or updating its corresponding notification attributes), as well as the potential
 *       object and object instance observables that share the same URI path elements.
 *
 * @param[in] object_id    Object identifier of the element.
 * @param[in] instance_id  Instance identifier of the element.
 * @param[in] resource_id  Resource identifier of the element.
 */
void lwm2m_observable_resource_value_changed(uint16_t object_id, uint16_t instance_id, uint16_t resource_id);

/**@brief Update ACL owner and server short server id in all objects.
 *
 * @param[in] old_ssid  Old short server id
 * @param[in] new_ssid  New short server id
 */
uint32_t lwm2m_update_acl_ssid(uint16_t old_ssid, uint16_t new_ssid);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_API_H__

/** @} */