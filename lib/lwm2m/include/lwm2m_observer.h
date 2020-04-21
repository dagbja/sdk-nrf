/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_OBSERVER_H__
#define LWM2M_OBSERVER_H__

#include <stdint.h>
#include <coap_api.h>
#include <lwm2m_api.h>

#define LWM2M_ATTR_TYPE_MIN_PERIOD        0 /**< p_min notification attribute type. */
#define LWM2M_ATTR_TYPE_MAX_PERIOD        1 /**< p_max notification attribute type. */
#define LWM2M_ATTR_TYPE_GREATER_THAN      2 /**< gt notification attribute type. */
#define LWM2M_ATTR_TYPE_LESS_THAN         3 /**< lt notification attribute type. */
#define LWM2M_ATTR_TYPE_STEP              4 /**< st notification attribute type. */

#define LWM2M_ATTR_MIN_PERIOD_CODE   0x01 /**< Bit mask for p_min notification attribute. */
#define LWM2M_ATTR_MAX_PERIOD_CODE   0x02 /**< Bit mask for p_max notification attribute. */
#define LWM2M_ATTR_GREATER_THAN_CODE 0x04 /**< Bit mask for gt notification attribute. */
#define LWM2M_ATTR_LESS_THAN_CODE    0x08 /**< Bit mask for lt notification attribute. */
#define LWM2M_ATTR_STEP_CODE         0x10 /**< Bit mask for st notification attribute. */

#define LWM2M_ATTR_OBJECT_LEVEL            1 /**< Object level notification attribute. */
#define LWM2M_ATTR_OBJECT_INSTANCE_LEVEL   2 /**< Object instance level notification attribute. */
#define LWM2M_ATTR_RESOURCE_LEVEL          3 /**< Resource level notification attribute. */
#define LWM2M_ATTR_RESOURCE_INSTANCE_LEVEL 4 /**< Resource instance level notification attribute. */

#define LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL  -1 /**< Notification attribute uninitialized assignment level. */
#define LWM2M_ATTR_DEFAULT_ASSIGNMENT_LEVEL  0 /**< Notification attribute default assignment level. */

#define LWM2M_MAX_NOTIF_ATTR_TYPE              5 /**< Number of supported notification attribute types. */
#define LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES 30 /**< Number of supported observable items which may have assigned notification attributes. */

#define LWM2M_OBSERVABLE_TYPE_INT         0x01 /**< Observable structure of integer type. */
#define LWM2M_OBSERVABLE_TYPE_STR         0x02 /**< Observable structure of string type. */
#define LWM2M_OBSERVABLE_TYPE_LIST        0x04 /**< Observable structure of lwm2m_list_t type. */
#define LWM2M_OBSERVABLE_TYPE_NO_CHECK    0x0E /**< Observable structure of a noncomparable data type. */

/**@brief LWM2M notification attribute structure. */
typedef struct
{
    union
    {
        int32_t i;                  /**< Used for p_min and p_max notification attributes. */
        float   f;                  /**< Used for gt, lt and st notification attributes. */
    } value;                        /**< Value assigned to the notification attribute. */
    int8_t assignment_level;        /**< Notification attribute assignment level. */
} lwm2m_notif_attr_t;

/**@brief LWM2M observable metadata structure.
 *
 * @details Structure used to identify the items that are being observed, as well as their assigned notification attributes.
 */
typedef struct
{
    uint16_t                 path[LWM2M_URI_PATH_MAX_LEN];               /**< URI path of the observable structure. */
    uint8_t                  path_len;                                   /**< Length of the URI path of the observable structure. */
    lwm2m_notif_attr_t       attributes[LWM2M_MAX_NOTIF_ATTR_TYPE]; /**< Notification attributes of this observable. */
    lwm2m_time_t             last_notification;                          /**< Time elapsed from the last notification sent. */
    int64_t                  con_notification;                           /**< Last time the notification has been sent as a confirmable message. */
    const void               *observable;                                /**< Pointer to the observable structure. */
    int32_t                  prev_value;                                 /**< The value of the observable structure that was reported in the last notification. */
    uint8_t                  type;                                       /**< Bitcode that identifies the data type of the observable. */
    uint8_t                  flags;                                      /**< Flags indicating whether the conditions defined by the attributes have been fulfilled. */
    uint16_t                 ssid;                                       /**< Short ID of the server that performed an OBSERVE or a WRITE-ATTRIBUTE request on the observable. */
    uint8_t                  changed;                                    /**< Flag indicating whether the value of the observable has changed since the last notification has been sent. */
} lwm2m_observable_metadata_t;

/**@brief Callback function to set the default notification attribute values.
 *
 * @details Will be called when a new notification attribute is initialized. LWM2M_ATTR_TYPE_MIN_PERIOD
 *          and LWM2M_ATTR_TYPE_MAX_PERIOD are always initialized upon observer registration.
 *
 * @param[in]    type            Notification attribute type whose default value is to be retrieved.
 * @param[inout] p_value         Pointer to the memory where the default value of the attribute is to be stored.
 * @param[in]    p_remote_server Structure containing address information and port number to the remote server
 *                               that made the OBSERVE request.
 */
typedef void (*lwm2m_observer_notif_attr_default_cb_t)(uint8_t type, void *p_value, struct nrf_sockaddr *p_remote_server);

/**@brief Callback function to retrieve a pointer to the value of the observable item.
 *
 * @param[in]    p_path     URI path that identifies the observable item.
 * @oaram[in]    path_len   Length of the URI path that identifies the observable item.
 * @param[inout] p_type     Pointer to the memory where the data type of the observable item is stored.
 *
 * @return  Pointer to the observable item data.
 */
typedef const void * (*lwm2m_observer_observable_get_cb_t)(const uint16_t *p_path, uint8_t path_len, uint8_t *p_type);

/**@brief Callback function to retrieve the uptime in milliseconds (ms).
 *
 * @return  Uptime in milliseconds (ms).
 */
typedef int64_t (*lwm2m_observer_uptime_get_cb_t)(void);

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
int lwm2m_observer_observable_init(struct nrf_sockaddr *p_remote,
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

/**@brief Handler function for incoming write-attribute requests from the LWM2M server.
 *
 * @param[in] p_path     URI path specified in the write-attributes request CoAP message.
 * @param[in] path_len   Length of the URI path specified in the write-attributes request CoAP message.
 * @param[in] p_request  The write-attributes CoAP request message.
 *
 * @return 0  If the notification attributes have been updated successfully or an error code on
 *            failure.
 */
int lwm2m_observer_write_attribute_handler(const uint16_t *p_path, uint8_t path_len, const coap_message_t *p_request);

/**@brief Set the callback function to set the default notification attribute values.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_observer_notif_attr_default_cb_set(lwm2m_observer_notif_attr_default_cb_t callback);

/**@brief Set the callback function to retrieve a pointer to the value of the observable item.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_observer_observable_get_cb_set(lwm2m_observer_observable_get_cb_t callback);

/**@brief Set the callback function to retrieve the uptime in milliseconds and initialize
 *        the reference timer that will be used to handle the information reporting interface.
 *
 * @param[in] callback  Callback function to be registered.
 */
void lwm2m_observer_uptime_cb_init(lwm2m_observer_uptime_get_cb_t callback);

/**@brief Restore the notification attributes of an observable item.
 *
 * @param[in] p_attributes  Array of notification attributes to be reassigned to the given observable.
 * @param[in] p_path        URI path that identifies the observable item.
 * @param[in] path_len      Length of the URI path that identifies the observable item.
 * @param[in] ssid          Short server ID of the observer.
 *
 * @retval 0        If the notification attributes have been reassigned successfully.
 * @retval -EINVAL  If the provided array or path is NULL.
 * @retval -EIO     If a callback function to reference the observable item has not been set yet.
 * @retval -ENOENT  If the observable or its corresponding metadata structure has not been found.
 */
int lwm2m_observer_notif_attr_restore(const lwm2m_notif_attr_t *p_attributes, const uint16_t *p_path, uint8_t path_len, uint16_t ssid);

/**@brief Retrieve the array of initialized observable metadata structures.
 *
 * @param[out] p_len  Size of the array.
 *
 * @return  Array of pointers to observable metadata structures.
 */
const lwm2m_observable_metadata_t * const * lwm2m_observer_observables_get(uint16_t *p_len);

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
void lwm2m_observer_notif_attr_storage_update(const uint16_t *p_path, uint16_t path_len, struct nrf_sockaddr *p_remote);

/**@brief Retrieve a pointer to an observable LwM2M item identified by its URI path.
 *
 * @param[in] p_path      URI path that identifies the observable item.
 * @param[in] path_len    Length of the URI path that identifies the observable item.
 *
 * @return  A valid pointer to the observable item if found, or NULL if the item has
 *          not been found or is not observable.
 */
const void * lwm2m_observer_observable_get(const uint16_t *p_path, uint8_t path_len);

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
void lwm2m_observer_resource_value_changed(uint16_t object_id, uint16_t instance_id, uint16_t resource_id);

#endif
