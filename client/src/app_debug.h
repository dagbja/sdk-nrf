/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef APP_DEBUG_H__
#define APP_DEBUG_H__

#include <stdint.h>

/**@brief Configurable device values. */
typedef struct {
    char imei[16];                /**< Static configured IMEI to overwrite value from SIP, used for debugging. */
    char msisdn[16];              /**< Static configured MSISDN to overwrite value from SIM, used for debugging. */
    char modem_logging[65];       /**< Modem logging: 0=off, 1=fidoless, 2=fido, other=XMODEMTRACE bitmap */
} debug_settings_t;

void app_debug_init(void);
void app_debug_clear(void);

const char * app_debug_imei_get(void);
int32_t app_debug_imei_set(const char * imei);

const char * app_debug_msisdn_get(void);
int32_t app_debug_msisdn_set(const char * msisdn);

const char * app_debug_modem_logging_get(void);
int32_t app_debug_modem_logging_set(const char * modem_logging);
void app_debug_modem_logging_enable(void);

#endif // APP_DEBUG_H__