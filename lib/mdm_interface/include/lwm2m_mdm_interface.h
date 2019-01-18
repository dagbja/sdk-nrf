/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
/**@file lwm2m_at_interface.h
 *
 * @brief Interface layer between the Modem and LWM2M.
 * @{
 */
#ifndef LWM2M_MDM_INTERFACE_H__
#define LWM2M_MDM_INTERFACE_H__

#include <stdint.h>

#include "datamodel/lwm2m_data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize module to interface modem.
 * @return 0 if success or an error code if module initialization fails.
 */
uint32_t lwm2m_mdm_interface_init();


/**
 * @brief Uninitialize module to interface modem.
 */
void lwm2m_mdm_interface_uninit();


/**
 * @brief Read the extended signal quality +CESQ value.
 *
 * @param[in] p_cesq_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cesq(lwm2m_model_cesq_rsp_t * const p_cesq_rsp);


#ifdef __cplusplus
}
#endif

#endif // LWM2M_MDM_INTERFACE_H__

/**@} */
