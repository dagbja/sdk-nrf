/*
 * Copyright (c) 2014 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m.h
 *
 * @defgroup iot_sdk_lwm2m_api LWM2M library private definitions.
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief LWM2M library private definitions.
 */

#ifndef LWM2M_H__
#define LWM2M_H__

#include <logging/log.h>
#define LOG_LEVEL CONFIG_NRF_LWM2M_LOG_LEVEL
LOG_MODULE_REGISTER(LWM2M_LOG_MODULE_NAME);

#include <zephyr.h>
#include <zephyr/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <net/coap_message.h>
#include <net/coap_codes.h>
#include <net/coap_observe_api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWM2M_TRC(...) LOG_DBG(__VA_ARGS__) /**< Used for getting trace of execution in the module. */
#define LWM2M_ERR(...) LOG_ERR(__VA_ARGS__) /**< Used for logging errors in the module. */

#define LWM2M_ENTRY() LWM2M_TRC(">> %s", __func__)
#define LWM2M_EXIT() LWM2M_TRC("<< %s", __func__)

/**
 * @defgroup iot_coap_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case the need to use an alternative architecture arises.
 * @{
 */
#define LWM2M_MUTEX_LOCK()   /*(void)k_mutex_lock(&m_lwm2m_mutex, K_FOREVER);*/ /**< Lock module using mutex */
#define LWM2M_MUTEX_UNLOCK() /*k_mutex_unlock(&m_lwm2m_mutex);*/                /**< Unlock module using mutex */
/** @} */

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          LWM2M_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
 *
 * @{
 */
#if (LWM2M_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                       \
    if ((PARAM) == NULL)                              \
    {                                                 \
        return EINVAL;                                \
    }
#else

#define NULL_PARAM_CHECK(PARAM)

#endif // LWM2M_DISABLE_API_PARAM_CHECK


/**
 * @defgroup iot_lwm2m_error Module's Error Macros
 * @details Macros used to return errors with the right base
 *
 * @{
 */
#define LWM2M_ERROR(PARAM) (PARAM | IOT_LWM2M_ERR_BASE)
/** @} */

#define LWM2M_REQUEST_TYPE_BOOTSTRAP  1
#define LWM2M_REQUEST_TYPE_REGISTER   2
#define LWM2M_REQUEST_TYPE_UPDATE     3
#define LWM2M_REQUEST_TYPE_DEREGISTER 4

/**@brief Memory allocator function.
 *
 * @param[in] size Size of memory to be used.
 *
 * @retval A valid memory address on success, else NULL.
 */
void * lwm2m_malloc(size_t size);

/**@brief Memory free function.
 *
 * @param[in] p_memory Address of memory to be freed.
 */
void lwm2m_free(void * p_memory);

/**@brief Function for encoding a uint16 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static inline uint8_t uint16_encode(uint16_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x00FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0xFF00) >> 8);
    return sizeof(uint16_t);
}

#ifdef __cplusplus
}
#endif

#endif // LWM2M_H__

/** @} */
