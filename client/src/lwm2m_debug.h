/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_DEBUG_H__
#define LWM2M_DEBUG_H__

#include <stdint.h>

/**@brief Configurable device values. */
typedef struct {
    char imei[16];                /**< Static configured IMEI to overwrite value from SIP, used for debugging. */
    char msisdn[16];              /**< Static configured MSISDN to overwrite value from SIM, used for debugging. */
    char modem_logging[65];       /**< Modem logging: 0=off, 1=fidoless, 2=fido, other=XMODEMTRACE bitmap */
} debug_settings_t;

void lwm2m_debug_init(void);
void lwm2m_debug_clear(void);

const char * lwm2m_debug_imei_get(void);
int32_t lwm2m_debug_imei_set(const char * imei);

const char * lwm2m_debug_msisdn_get(void);
int32_t lwm2m_debug_msisdn_set(const char * msisdn);

const char * lwm2m_debug_modem_logging_get(void);
int32_t lwm2m_debug_modem_logging_set(const char * modem_logging);

#endif // LWM2M_DEBUG_H__