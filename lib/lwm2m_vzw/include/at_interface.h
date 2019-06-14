/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

#include <stdint.h>
#include <stdbool.h>

/* TODO: Move APIs to modem interface modem with data model. */

void mdm_interface_init();

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

#endif // AT_INTERFACE_H__
