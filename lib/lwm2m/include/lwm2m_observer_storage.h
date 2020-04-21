/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <coap_api.h>
#include <coap_observe_api.h>
#include <lwm2m_observer.h>

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
