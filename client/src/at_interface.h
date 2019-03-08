/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

void at_read_imei_and_msisdn(void);
void at_send_command(const char *at_command, bool do_logging);

#endif // AT_INTERFACE_H__