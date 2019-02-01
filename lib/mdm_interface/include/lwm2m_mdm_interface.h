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


/**
 * @brief Read the network registration status +CEREG value.
 *
 * @param[in] p_cereg_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cereg(lwm2m_model_cereg_rsp_t * const p_cereg_rsp);


/**
 * @brief Read the PLMN selection +COPS value.
 *
 * @param[in] p_cops_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cops(lwm2m_model_cops_rsp_t * const p_cops_rsp);


/**
 * @brief Read the subscriber number +CNUM.
 *
 * @param[in] p_cnum_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cnum(lwm2m_model_cnum_rsp_t * const p_cnum_rsp);


/**
 * @brief Read the manufacturer identification +CGMI.
 *
 * @param[in] p_cgmi_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cgmi(lwm2m_model_cgmi_rsp_t * const p_cgmi_rsp);


/**
 * @brief Read the model identification +CGMM.
 *
 * @param[in] p_cgmm_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cgmm(lwm2m_model_cgmm_rsp_t * const p_cgmm_rsp);


/**
 * @brief Read the revision identification +CGMR.
 *
 * @param[in] p_cgmr_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cgmr(lwm2m_model_cgmr_rsp_t * const p_cgmr_rsp);


/**
 * @brief Read the product serial number identification +CGSN.
 *
 * @param[in] p_cgsn_rsp Data model structure pointer to fill in.
 * @return 0 if success or any error code.
 */
uint32_t lwm2m_mdm_interface_read_cgsn(lwm2m_model_cgsn_rsp_t * const p_cgsn_rsp);


#ifdef __cplusplus
}
#endif

#endif // LWM2M_MDM_INTERFACE_H__

/**@} */
