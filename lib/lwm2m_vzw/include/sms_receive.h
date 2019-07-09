/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef SMS_RECEIVE_H__
#define SMS_RECEIVE_H__

/**
 * @brief Enable SMS support using an AT command.
 * 
 * When SMS are received, an AT notification will be received and parsed.
 * SMS support is not available by default.
 * 
 * @return 0 if success or and error if the AT command to enable SMS failed.
 */
int32_t sms_receiver_init(void);

/**
 * @brief Parse an AT notification and decode its content if possible.
 *
 * @param[in] notif NUll-terminated AT notification
 */
void sms_receiver_notif_parse(char *notif);

/**
 * @return The number of valid SMS decoded.
 */
uint32_t sms_receive_counter(void);

#endif // SMS_RECEIVE_H__