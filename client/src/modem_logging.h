/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef MODEM_LOGGING_H__
#define MODEM_LOGGING_H__

#include <stdint.h>

/**
 * @brief Send an null-terminated AT command to the Modem.
 *
 * @param[in] cmd Pointer to the null-terminated AT command to send.
 * @param[in] do_logging Set to true to print the AT command response.
 * @return An error code if the command write or response read failed.
 */
int modem_at_write(const char *const cmd, bool do_logging);

int modem_logging_init(void);
const char * modem_logging_get(void);
int32_t modem_logging_set(const char * new_modem_logging);
void modem_trace_enable(void);
void modem_logging_enable(void);

#endif // MODEM_LOGGING_H__
