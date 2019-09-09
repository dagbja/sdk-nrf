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

#include <net/socket.h>
#include <coap_observe_api.h>

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
    uint64_t       msisdn;              /**< SMS number MSISDN. */
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
 * @brief LWMW2M operation code and invalid object/instance definitions.
 */

/**
 * @warning The invalid resource and instance are not stated by the lwm2m spec as reserved and will
 *          cause issues if instances or resources with these IDs is added.
 */
#define LWM2M_NAMED_OBJECT            65535 /**< Flag to indicate that the object does not use Integer as object id. */
#define LWM2M_INVALID_RESOURCE        65535 /**< Invalid Resource ID. */
#define LWM2M_INVALID_INSTANCE        65535 /**< Invalid Instance ID. */

#define LWM2M_PERMISSION_READ         0x01  /**< Bit mask for LWM2M read permission. */
#define LWM2M_PERMISSION_WRITE        0x02  /**< Bit mask for LWM2M write permission. */
#define LWM2M_PERMISSION_EXECUTE      0x04  /**< Bit mask for LWM2M execute permission. */
#define LWM2M_PERMISSION_DELETE       0x08  /**< Bit mask for LWM2M delete permission. */
#define LWM2M_PERMISSION_CREATE       0x10  /**< Bit mask for LWM2M create permission. */
#define LWM2M_PERMISSION_OBSERVE      0x40  /**< Bit mask for LWM2M observe permission. */

#define LWM2M_OPERATION_CODE_NONE     0x00                     /**< Bit mask for LWM2M no operation. */
#define LWM2M_OPERATION_CODE_READ     LWM2M_PERMISSION_READ    /**< Bit mask for LWM2M read operation. */
#define LWM2M_OPERATION_CODE_WRITE    LWM2M_PERMISSION_WRITE   /**< Bit mask for LWM2M write operation. */
#define LWM2M_OPERATION_CODE_EXECUTE  LWM2M_PERMISSION_EXECUTE /**< Bit mask for LWM2M execute operation. */
#define LWM2M_OPERATION_CODE_DELETE   LWM2M_PERMISSION_DELETE  /**< Bit mask for LWM2M delete operation. */
#define LWM2M_OPERATION_CODE_CREATE   LWM2M_PERMISSION_CREATE  /**< Bit mask for LWM2M create operation. */
#define LWM2M_OPERATION_CODE_DISCOVER 0x20                     /**< Bit mask for LWM2M discover operation. */
#define LWM2M_OPERATION_CODE_OBSERVE  LWM2M_PERMISSION_OBSERVE /**< Bit mask for LWM2M observe operation. */
/**@} */

/**@cond */
// Forward declare structs.
typedef struct lwm2m_object_t   lwm2m_object_t;
typedef struct lwm2m_instance_t lwm2m_instance_t;
/**@endcond */

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
                        struct sockaddr           * p_remote,
                        uint8_t                     coap_code,
                        uint32_t                    err_code);


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
 * @retval NRF_SUCCESS If initialization was successful.
 */
uint32_t lwm2m_init();

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
uint32_t lwm2m_bootstrap(struct sockaddr         * p_remote,
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
uint32_t lwm2m_register(struct sockaddr         * p_remote,
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
uint32_t lwm2m_update(struct sockaddr         * p_remote,
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
uint32_t lwm2m_deregister(struct sockaddr * p_remote, coap_transport_handle_t transport);

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
 * @param[inout] p_buffer     Pointer to a buffer to fill with link format encoded string. If
 *                            a NULL pointer is provided the function will dry-run the function
 *                            in order to calculate how much memory that is needed for the link
 *                            format string.
 * @param[inout] p_buffer_len As input used to indicate the length of the buffer. It will return the
 *                            used amount of buffer length by reference in response. If NULL pointer
 *                            is provided for p_buffer, the value by reference output will be the number
 *                            of bytes needed to generate the link format string.
 *
 * @retval        NRF_SUCCESS      If generation of link format string was successful.
 * @retval        NRF_ERROR_NO_MEM If the provided memory was not large enough.
 */
uint32_t lwm2m_coap_handler_gen_link_format(uint8_t * p_buffer, uint16_t * p_buffer_len);

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


uint32_t lwm2m_observe_register(uint8_t             * p_payload,
                                uint16_t              payload_len,
                                uint16_t              max_age,
                                coap_message_t      * p_request,
                                coap_content_type_t   content_type,
                                void                * p_resource);

/**@brief Unregister observer if found.
 *
 * @param p_remote           Remote address of the observing server.
 * @param p_resource   Pointer to the resource of interest registered.
 *
 * @retval 0      If the observable resource was unregistered successfully.
 * @retval ENOMEM If the observable resource could not be found in the list.
 * @retval EINVAL If one of the parameters is a NULL pointer.
 */
uint32_t lwm2m_observe_unregister(struct sockaddr  * p_remote,
                                  void             * p_resource);

uint32_t lwm2m_notify(uint8_t         * p_payload,
                      uint16_t          payload_len,
                      coap_observer_t * p_observer,
                      coap_msg_type_t   type);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_API_H__

/** @} */
