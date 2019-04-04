/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef APP_DEBUG_H__
#define APP_DEBUG_H__

#include <stdint.h>

#define DEBUG_FLAG_DISABLE_PSM  0x01       /**< Set if disable PSM. */
#define DEBUG_FLAG_SMS_SUPPORT  0x02       /**< Set if enable SMS support. */

/**@brief Configurable device values. */
typedef struct {
    char imei[16];                /**< Static configured IMEI to overwrite value from SIP, used for debugging. */
    char msisdn[16];              /**< Static configured MSISDN to overwrite value from SIM, used for debugging. */
    char modem_logging[65];       /**< Modem logging: 0=off, 1=fidoless, 2=fido, other=XMODEMTRACE bitmap */
    uint32_t flags;               /**< Flags to control application behaviour. */
} debug_settings_t;

void app_debug_init(void);
void app_debug_clear(void);

const char * app_debug_imei_get(void);
int32_t app_debug_imei_set(const char * imei);

const char * app_debug_msisdn_get(void);
int32_t app_debug_msisdn_set(const char * msisdn);

bool app_debug_flag_is_set(uint32_t flag);
int32_t app_debug_flag_set(uint32_t flag);
int32_t app_debug_flag_clear(uint32_t flag);

const char * app_debug_modem_logging_get(void);
int32_t app_debug_modem_logging_set(const char * modem_logging);
void app_debug_modem_logging_enable(bool modem_initialized);

#endif // APP_DEBUG_H__