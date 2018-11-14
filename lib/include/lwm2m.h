/*$$$LICENCE_NORDIC_STANDARD<2015>$$$*/
/** @file lwm2m.h
 *
 * @defgroup iot_sdk_lwm2m_api LWM2M library private definitions.
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief LWM2M library private definitions.
 */

#ifndef LWM2M_H__
#define LWM2M_H__

#include "stdint.h"
#include "stdbool.h"
#include "coap_message.h"
#include "coap_codes.h"
#include "sdk_config.h"
#include "sdk_os.h"
#include "iot_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup iot_coap_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case the need to use an alternative architecture arises.
 * @{
 */
#define LWM2M_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_lwm2m_mutex)   /**< Lock module using mutex */
#define LWM2M_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_lwm2m_mutex) /**< Unlock module using mutex */
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
        return (NRF_ERROR_NULL | IOT_LWM2M_ERR_BASE); \
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
void * lwm2m_malloc(uint32_t size);

/**@brief Memory free function.
 *
 * @param[in] p_memory Address of memory to be freed.
 */
void lwm2m_free(void * p_memory);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_H__

/** @} */
