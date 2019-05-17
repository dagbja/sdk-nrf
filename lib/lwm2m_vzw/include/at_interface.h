/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

#include <stdbool.h>
#include <lwm2m_api.h>

/* TODO: Move APIs to modem interface modem with data model. */

int mdm_interface_init(void);

/**
 * @brief Send an null-terminated AT command to the Modem.
 *
 * @param[in] cmd Pointer to the null-terminated AT command to send.
 * @param[in] do_logging Set to true to print the AT command response.
 * @return An error code if the command write or response read failed.
 */
int mdm_interface_at_write(const char *const cmd, bool do_logging);

int at_apn_setup_wait_for_ipv6(char * apn);
int at_read_imei_and_msisdn(char *p_imei, int imei_len, char *p_msisdn, int msisdn_len);
int at_read_sim_iccid(char *p_iccid, uint32_t * p_iccid_len);
int at_read_firmware_version(char *p_fw_version, uint32_t * p_fw_version_len);
int at_read_operator_id(uint32_t *p_oper_id);
int at_read_net_reg_stat(uint32_t * p_net_stat);
int at_read_manufacturer(lwm2m_string_t * p_manufacturer_id);
int at_read_model_number(lwm2m_string_t * p_model_number);
int at_read_radio_signal_strength(int32_t * p_signal_strength);
int at_read_cell_id(uint32_t * p_cell_id);
int at_read_smnc_smcc(int32_t * p_smnc, int32_t *p_smcc);
int at_read_time(int32_t * p_time, int32_t * p_utc_offset);

#endif // AT_INTERFACE_H__
