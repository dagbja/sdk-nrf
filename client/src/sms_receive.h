/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef SMS_RECEIVE_H__
#define SMS_RECEIVE_H__

uint32_t sms_receive_counter(void);
void sms_receive_thread_start(void);

#endif // SMS_RECEIVE_H__