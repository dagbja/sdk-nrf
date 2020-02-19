/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <lwm2m_instance_storage.h>
#include <app_debug.h>
#include <at_interface.h>
#include <lwm2m_carrier_main.h>


static debug_settings_t m_debug_settings;

void app_debug_init(void)
{
    (void)lwm2m_debug_settings_load(&m_debug_settings);

    if (m_debug_settings.coap_con_interval != 0) {
        lwm2m_coap_con_interval_set(m_debug_settings.coap_con_interval);
    }
}

void lwm2m_debug_reset(void)
{
    memset(&m_debug_settings, 0, sizeof(m_debug_settings));

    lwm2m_debug_settings_store(&m_debug_settings);
}

int32_t lwm2m_debug_con_interval_set(int64_t con_interval)
{
    m_debug_settings.coap_con_interval = con_interval;

    return lwm2m_debug_settings_store(&m_debug_settings);
}

int64_t lwm2m_debug_con_interval_get(void)
{
    return m_debug_settings.coap_con_interval;
}

int32_t lwm2m_debug_operator_id_set(uint32_t operator_id)
{
    m_debug_settings.operator_id = operator_id;

    return lwm2m_debug_settings_store(&m_debug_settings);
}

uint32_t lwm2m_debug_operator_id_get(void)
{
    return m_debug_settings.operator_id;
}

bool lwm2m_debug_is_set(uint32_t flag)
{
    return ((m_debug_settings.flags & flag) == flag);
}

int32_t lwm2m_debug_set(uint32_t flag)
{
    m_debug_settings.flags |= flag;

    return lwm2m_debug_settings_store(&m_debug_settings);
}

int32_t lwm2m_debug_clear(uint32_t flag)
{
    m_debug_settings.flags &= ~flag;

    return lwm2m_debug_settings_store(&m_debug_settings);
}
