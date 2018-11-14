/*$$$LICENCE_NORDIC_STANDARD<2015>$$$*/
/** @file lwm2m_bootstrap.h
 *
 * @defgroup iot_sdk_lwm2m_bootstrap_api LWM2M bootstrap API interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief Bootstrap API interface for the LWM2M protocol.
 */

#ifndef LWM2M_BOOTSTRAP_H__
#define LWM2M_BOOTSTRAP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Initialize the LWM2M register module.
 *
 * @details Calling this function will set the module in default state.
 */
uint32_t internal_lwm2m_bootstrap_init(void);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_BOOTSTRAP_H__

/**@} */
