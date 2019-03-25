/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

int at_read_imei_and_msisdn(char *p_imei, int imei_len, char *p_msisdn, int msisdn_len);
int at_read_sim_iccid(char *p_iccid, int iccid_len);
int at_send_command(const char *at_command, bool do_logging);

#endif // AT_INTERFACE_H__