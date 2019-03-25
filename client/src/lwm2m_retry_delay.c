/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <lwm2m_api.h>
#include <lwm2m_retry_delay.h>

static uint8_t m_retry_count[1+LWM2M_MAX_SERVERS];

// Verizon specific retry delays in seconds.
// TODO: Add retry handling for other vendors than Verizon.
static s32_t m_retry_delay[] = { 2*60, 4*60, 6*60, 8*60, 24*60*60 };

s32_t lwm2m_retry_delay_get(int instance_id, bool next_delay)
{
    s32_t retry_delay;

    if (instance_id == 0 && m_retry_count[instance_id] == sizeof m_retry_delay - 1) {
         // Bootstrap retry does not use the last retry value and does not continue before next power up.
         return -1;
    } else if (instance_id >= 1+LWM2M_MAX_SERVERS) {
        // Illegal instance_id.
        return -1;
    }

    if (m_retry_count[instance_id] == sizeof m_retry_delay) {
        // Retry counter wrap around.
        m_retry_count[instance_id] = 0;
    }

    if (next_delay) {
        // Fetch next retry delay.
        retry_delay = m_retry_delay[m_retry_count[instance_id]];
        m_retry_count[instance_id]++;
    } else if (m_retry_count[instance_id] > 0) {
        // Fetch current retry delay.
        retry_delay = m_retry_delay[m_retry_count[instance_id] - 1];
    } else {
        // No retry delay started.
        retry_delay = -1;
    }

    return retry_delay;
}

void lwm2m_retry_delay_reset(int instance_id)
{
    m_retry_count[instance_id] = 0;
}