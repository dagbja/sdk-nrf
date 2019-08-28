/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef APP_DEBUG_H__
#define APP_DEBUG_H__

#include <stdint.h>

#define DEBUG_FLAG_DISABLE_IPv6 0x08       /**< Set if disable IPv6. */
#define DEBUG_FLAG_DISABLE_FALLBACK  0x10  /**< Set if disable IP fallback. */

/**@brief Configurable device values. */
typedef struct {
    char dummy1[16];              /**< Currently unused value, previous used for static configured IMEI. */
    char dummy2[16];              /**< Currently unused value, previous used for static configured MSISDN. */
    char modem_logging[65];       /**< Modem logging: 0=off, 1=fidoless, 2=fido, other=XMODEMTRACE bitmap */
    uint32_t flags;               /**< Flags to control application behaviour. */
} debug_settings_t;

void app_debug_init(void);
void lwm2m_debug_clear(void);

bool lwm2m_debug_flag_is_set(uint32_t flag);
int32_t lwm2m_debug_flag_set(uint32_t flag);
int32_t lwm2m_debug_flag_clear(uint32_t flag);

const char * lwm2m_debug_modem_logging_get(void);
int32_t lwm2m_debug_modem_logging_set(const char * modem_logging);
void lwm2m_debug_modem_logging_enable(void);

#endif // APP_DEBUG_H__
