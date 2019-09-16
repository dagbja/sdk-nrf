/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef APP_DEBUG_H__
#define APP_DEBUG_H__

#include <stdint.h>

#define LWM2M_DEBUG_DISABLE_CARRIER_CHECK  0x04  /**< Set if disable carrier check. */
#define LWM2M_DEBUG_DISABLE_IPv6           0x08  /**< Set if disable IPv6. */
#define LWM2M_DEBUG_DISABLE_FALLBACK       0x10  /**< Set if disable IP fallback. */

/**@brief Configurable device values. */
typedef struct {
    char dummy1[16];              /**< Currently unused value, previous used for static configured IMEI. */
    char dummy2[16];              /**< Currently unused value, previous used for static configured MSISDN. */
    char dummy3[65];              /**< Currently unused value, previous used for modem logging. */
    uint32_t flags;               /**< Flags to control application behaviour. */
} debug_settings_t;

void app_debug_init(void);
void lwm2m_debug_reset(void);

bool lwm2m_debug_is_set(uint32_t flag);
int32_t lwm2m_debug_set(uint32_t flag);
int32_t lwm2m_debug_clear(uint32_t flag);

#endif // APP_DEBUG_H__
