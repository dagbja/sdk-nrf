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


static debug_settings_t m_debug_settings;

void app_debug_init(void)
{
    (void)lwm2m_debug_settings_load(&m_debug_settings);
}

void lwm2m_debug_clear(void)
{
    memset(&m_debug_settings, 0, sizeof(m_debug_settings));

    lwm2m_debug_settings_store(&m_debug_settings);
}

bool lwm2m_debug_flag_is_set(uint32_t flag)
{
    return ((m_debug_settings.flags & flag) == flag);
}

int32_t lwm2m_debug_flag_set(uint32_t flag)
{
    m_debug_settings.flags |= flag;

    return lwm2m_debug_settings_store(&m_debug_settings);
}

int32_t lwm2m_debug_flag_clear(uint32_t flag)
{
    m_debug_settings.flags &= ~flag;

    return lwm2m_debug_settings_store(&m_debug_settings);
}
