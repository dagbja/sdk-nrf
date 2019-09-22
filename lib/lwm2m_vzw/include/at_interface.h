/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

#include <stdbool.h>
#include <lwm2m_api.h>


typedef void (*at_net_reg_stat_cb_t)(uint32_t net_stat);

/* TODO: Move APIs to modem interface modem with data model. */

int mdm_interface_init(void);

int at_apn_setup_wait_for_ipv6(char * apn);

/**
 * @brief Read device IMEI.
 * IMEI is always 14 digits and a check digit.
 *
 * @param p_imei[in] Buffer to store IMEI. Stored as a NULL-terminated String with 15 digits.
 * @param imei_len[in] Size of the buffer. Must be at least 16 bytes.
 *
 * @return Result of the IMEI read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval 0 Success.
 */
int at_read_imei(char * const p_imei, int imei_len);

/**
 * @brief Read subscriber number (MSISDN).
 * MSISDN is maximum 15 digits. Length may varies based on the operator.
 * MSISDN may not always be available depending on the SIM card.
 *
 * @param p_msisdn[in] Buffer to store IMEI. Store a NULL-terminated String with max 15 digits MSISDN.
 * @param msisdn_len[in] Size of the buffer. Must be at least 16 bytes.
 *
 * @return Result of the MSISDN read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval EPERM No MSISDN available on the SIM card.
 * @retval 0 Success.
 */
int at_read_msisdn(char * const p_msisdn, int msisdn_len);

int at_read_sim_iccid(char *p_iccid, uint32_t * p_iccid_len);

/**
 * @brief Read the Modem firmware version name.
 *
 * Version name is something like "mfw_nrf9160_0.7.0-15.alpha".
 */
int at_read_firmware_version(char *p_fw_version, uint32_t * p_fw_version_len);

int at_read_operator_id(uint32_t *p_oper_id);
int at_read_net_reg_stat(uint32_t * p_net_stat);
int at_read_manufacturer(lwm2m_string_t * p_manufacturer_id);
int at_read_model_number(lwm2m_string_t * p_model_number);
int at_read_radio_signal_strength_and_link_quality(int32_t * p_signal_strength, int32_t * p_link_quality);
int at_read_cell_id(uint32_t * p_cell_id);
int at_read_smnc_smcc(int32_t * p_smnc, int32_t *p_smcc);

/**@brief Read time from modem using AT+CCLK? at command.
 *
 * @param[out] p_time       Pointer to time since Epoch
 * @param[out] p_utc_offset Pointer to UTC offset
 *
 * @return Pointer to timezone string or NULL in case lookup failed
 *
 * */
int at_read_time(int32_t * p_time, int32_t * p_utc_offset);
int at_read_ipaddr(lwm2m_list_t * p_ipaddr_list);

void at_subscribe_net_reg_stat(at_net_reg_stat_cb_t net_reg_stat_cb);

#endif // AT_INTERFACE_H__
