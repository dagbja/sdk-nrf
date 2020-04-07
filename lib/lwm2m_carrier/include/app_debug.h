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
#define LWM2M_DEBUG_PSK_MAX_LEN              32  /**< Maximum length of the debug PSK stored in flash. */       

/**@brief Configurable device values. */
typedef struct {
    int64_t coap_con_interval;                      /**< When to send CON instead of NON in CoAP observables. */
    uint32_t operator_id;                           /**< Used to set a specific operator behaviour. */
    uint8_t bootstrap_psk[LWM2M_DEBUG_PSK_MAX_LEN]; /**< Used to replace our static pre-shared key. */
    size_t bootstrap_psk_len;                       /**< Length of the Pre-Shared Key. */
    char dummy[49];                                 /**< Currently unused value. */
    uint32_t flags;                                 /**< Flags to control application behaviour. */
} debug_settings_t;

void app_debug_init(void);
void lwm2m_debug_reset(void);

int32_t lwm2m_debug_con_interval_set(int64_t con_interval);
int64_t lwm2m_debug_con_interval_get(void);

int32_t lwm2m_debug_operator_id_set(uint32_t operator_id);
uint32_t lwm2m_debug_operator_id_get(void);

/**
 * @brief Function for writing a pre-shared key (PSK) to the debug settings, and store the settings to flash.
 *
 * @param[in] p_psk Bootstrap PSK to write to debug settings.
 *
 * @return Negative value on failure, or positive value (number of bytes written) on success.
 */
int32_t lwm2m_debug_bootstrap_psk_set(const lwm2m_string_t * const p_psk);

/**
 * @brief Function for reading a pre-shared key (PSK) from the debug settings.
 *
 * @param[in/out] p_psk Memory where the bootstrap PSK will be stored.
 *
 * @return 0 if success.
 *         -EINVAL if the pointer provided is NULL
 *         -ENOENT if the debug psk does not exists.
 */
int32_t lwm2m_debug_bootstrap_psk_get(lwm2m_string_t *p_psk);

bool lwm2m_debug_is_set(uint32_t flag);
int32_t lwm2m_debug_set(uint32_t flag);
int32_t lwm2m_debug_clear(uint32_t flag);

#endif // APP_DEBUG_H__
