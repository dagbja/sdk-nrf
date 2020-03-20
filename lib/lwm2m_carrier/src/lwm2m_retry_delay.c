/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdbool.h>

#include <lwm2m_api.h>
#include <lwm2m_retry_delay.h>
#include <lwm2m_conn_ext.h>
#include <lwm2m_carrier_main.h>
#include <operator_check.h>

/** Verizon specific PDN activation delays. */
static int32_t m_pdn_retry_delay_vzw[] =
{
    K_SECONDS(2),
    K_MINUTES(1),
    K_MINUTES(30)
};

/** PDN activation count. */
static int32_t m_retry_count_pdn;

/** Verizon specific retry delays. */
static int32_t m_retry_delay_vzw[] =
{
    K_MINUTES(2),
    K_MINUTES(4),
    K_MINUTES(6),
    K_MINUTES(8),
    K_HOURS(24)
};

/** Connection retry count. */
static uint8_t m_retry_count_connect[1+LWM2M_MAX_SERVERS];

static int32_t lwm2m_retry_delay_pdn_vzw_get(bool * p_is_last)
{
    int retry_delay = m_pdn_retry_delay_vzw[m_retry_count_pdn];

    if (m_retry_count_pdn < (ARRAY_SIZE(m_pdn_retry_delay_vzw) - 1)) {
        m_retry_count_pdn++;
    } else if (p_is_last) {
        *p_is_last = true;
    }

    return retry_delay;
}

static int32_t lwm2m_retry_delay_pdn_att_get(uint16_t apn_instance, bool * p_is_last)
{
    uint8_t apn_retries = lwm2m_conn_ext_apn_retries_get(0, apn_instance);
    int32_t retry_delay;

    if (m_retry_count_pdn == apn_retries) {
        // Retry counter wrap around.
        m_retry_count_pdn = 0;
        retry_delay = K_SECONDS(lwm2m_conn_ext_apn_retry_back_off_period_get(0, apn_instance));
    } else {
        m_retry_count_pdn++;
        retry_delay = K_SECONDS(lwm2m_conn_ext_apn_retry_period_get(0, apn_instance));
    }

    if (p_is_last) {
        *p_is_last = (m_retry_count_pdn == 0);
    }

    return retry_delay;
}

static int32_t lwm2m_retry_delay_vzw_get(uint16_t security_instance, bool * p_is_last)
{
    int32_t retry_delay;

    if (security_instance < 0 || security_instance >= 1+LWM2M_MAX_SERVERS) {
        // Illegal security_instance.
        return -1;
    }

    if (m_retry_count_connect[security_instance] > 0) {
        // Fetch current retry delay.
        retry_delay = m_retry_delay_vzw[m_retry_count_connect[security_instance] - 1];
    } else {
        // No retry delay started.
        retry_delay = -1;
    }

    if (p_is_last && (m_retry_count_connect[security_instance] == ARRAY_SIZE(m_retry_delay_vzw))) {
        *p_is_last = true;
    }

    return retry_delay;
}

static int32_t lwm2m_retry_delay_vzw_next(uint16_t security_instance, bool * p_is_last)
{
    int32_t retry_delay;

    if (security_instance < 0 || security_instance >= 1+LWM2M_MAX_SERVERS) {
        // Illegal security_instance.
        return -1;
    }

    if (security_instance == 0 && m_retry_count_connect[security_instance] == ARRAY_SIZE(m_retry_delay_vzw) - 1) {
        // Bootstrap retry does not use the last retry value and does not continue before next power up.
        return -1;
    }

    if (m_retry_count_connect[security_instance] == ARRAY_SIZE(m_retry_delay_vzw)) {
        // Retry counter wrap around.
        m_retry_count_connect[security_instance] = 0;
    }

    // Fetch next retry delay.
    retry_delay = m_retry_delay_vzw[m_retry_count_connect[security_instance]];
    m_retry_count_connect[security_instance]++;

    if (p_is_last && (m_retry_count_connect[security_instance] == ARRAY_SIZE(m_retry_delay_vzw))) {
        *p_is_last = true;
    }

    return retry_delay;
}

static int32_t lwm2m_retry_delay_att_get(uint16_t security_instance, bool * p_is_last)
{
    // TODO: Handle DTLS handshake retry delays for AT&T
    return K_MINUTES(2);
}

static int32_t lwm2m_retry_delay_att_next(uint16_t security_instance, bool * p_is_last)
{
    // TODO: Handle DTLS handshake retry delays for AT&T
    return K_MINUTES(2);
}

int32_t lwm2m_retry_delay_pdn_get(uint16_t apn_instance, bool * p_is_last)
{
    int32_t retry_delay = 0;

    if (operator_is_vzw(true)) {
        retry_delay = lwm2m_retry_delay_pdn_vzw_get(p_is_last);
    } else if (operator_is_att(true)) {
        retry_delay = lwm2m_retry_delay_pdn_att_get(apn_instance, p_is_last);
    }

    return retry_delay;
}

void lwm2m_retry_delay_pdn_reset(void)
{
    m_retry_count_pdn = 0;
}

int32_t lwm2m_retry_delay_connect_get(uint16_t security_instance, bool * p_is_last)
{
    int32_t retry_delay = 0;

    if (operator_is_vzw(true)) {
        retry_delay = lwm2m_retry_delay_vzw_get(security_instance, p_is_last);
    } else if (operator_is_att(true)) {
        retry_delay = lwm2m_retry_delay_att_get(security_instance, p_is_last);
    }

    return retry_delay;
}

int32_t lwm2m_retry_delay_connect_next(uint16_t security_instance, bool * p_is_last)
{
    int32_t retry_delay = 0;

    if (operator_is_vzw(true)) {
        retry_delay = lwm2m_retry_delay_vzw_next(security_instance, p_is_last);
    } else if (operator_is_att(true)) {
        retry_delay = lwm2m_retry_delay_att_next(security_instance, p_is_last);
    }

    return retry_delay;
}

void lwm2m_retry_delay_connect_reset(uint16_t security_instance)
{
    m_retry_count_connect[security_instance] = 0;
}
