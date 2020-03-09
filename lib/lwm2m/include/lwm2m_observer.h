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

#define LWM2M_ATTRIBUTE_MIN_PERIOD_CODE   0x01 /**< Bit mask for p_min notification attribute. */
#define LWM2M_ATTRIBUTE_MAX_PERIOD_CODE   0x02 /**< Bit mask for p_max notification attribute. */
#define LWM2M_ATTRIBUTE_GREATER_THAN_CODE 0x04 /**< Bit mask for gt notification attribute. */
#define LWM2M_ATTRIBUTE_LESS_THAN_CODE    0x08 /**< Bit mask for lt notification attribute. */
#define LWM2M_ATTRIBUTE_STEP_CODE         0x10 /**< Bit mask for st notification attribute. */

#define LWM2M_ATTR_OBJECT_LEVEL            1 /**< Object level notification attribute. */
#define LWM2M_ATTR_OBJECT_INSTANCE_LEVEL   2 /**< Object instance level notification attribute. */
#define LWM2M_ATTR_RESOURCE_LEVEL          3 /**< Resource level notification attribute. */
#define LWM2M_ATTR_RESOURCE_INSTANCE_LEVEL 4 /**< Resource instance level notification attribute. */

#define LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL  -1 /**< Notification attribute uninitialized assignment level. */
#define LWM2M_ATTR_DEFAULT_ASSIGNMENT_LEVEL  0 /**< Notification attribute default assignment level. */

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
int lwm2m_observable_notif_attributes_restore(const lwm2m_notif_attribute_t *p_attributes, const uint16_t *p_path, uint8_t path_len, uint16_t ssid);

/**@brief Retrieve the array of initialized observable metadata structures.
 *
 * @param[out] p_len  Size of the array.
 *
 * @return  Array of pointers to observable metadata structures.
 */
const lwm2m_observable_metadata_t * const * lwm2m_observables_get(uint16_t *p_len);

#endif
