/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef APP_DEBUG_H__
#define APP_DEBUG_H__

#include <stdint.h>

#define LWM2M_DEBUG_ROAM_AS_HOME           0x02  /**< Set if Roaming as Home. */
#define LWM2M_DEBUG_DISABLE_CARRIER_CHECK  0x04  /**< Set if disable carrier check. */
#define LWM2M_DEBUG_DISABLE_IPv6           0x08  /**< Set if disable IPv6. */
#define LWM2M_DEBUG_DISABLE_FALLBACK       0x10  /**< Set if disable IP fallback. */

/**@brief Configurable device values. */
typedef struct {
    int64_t coap_con_interval;    /**< When to send CON instead of NON in CoAP observables. */
    uint32_t operator_id;         /**< Used to set a specific operator behaviour. */
    char dummy1[4];               /**< Currently unused value, previous used for static configured IMEI. */
    char dummy2[16];              /**< Currently unused value, previous used for static configured MSISDN. */
    char dummy3[65];              /**< Currently unused value, previous used for modem logging. */
    uint32_t flags;               /**< Flags to control application behaviour. */
} debug_settings_t;

void app_debug_init(void);
void lwm2m_debug_reset(void);

int32_t lwm2m_debug_con_interval_set(int64_t con_interval);
int64_t lwm2m_debug_con_interval_get(void);

int32_t lwm2m_debug_operator_id_set(uint32_t operator_id);
uint32_t lwm2m_debug_operator_id_get(void);

bool lwm2m_debug_is_set(uint32_t flag);
int32_t lwm2m_debug_set(uint32_t flag);
int32_t lwm2m_debug_clear(uint32_t flag);

#endif // APP_DEBUG_H__
