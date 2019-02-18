/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_debug

#include <stdint.h>
#include <string.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_debug.h>


static debug_settings_t m_debug_settings;

void lwm2m_debug_init(void)
{
    lwm2m_debug_settings_load(&m_debug_settings);
}

void lwm2m_debug_clear(void)
{
    memset(&m_debug_settings, 0, sizeof(m_debug_settings));
    lwm2m_debug_settings_store(&m_debug_settings);
}

const char * lwm2m_debug_imei_get(void)
{
    return m_debug_settings.imei;
}

int32_t lwm2m_debug_imei_set(const char * imei)
{
    memset(m_debug_settings.imei, 0, sizeof(m_debug_settings.imei));
    strncpy(m_debug_settings.imei, imei, sizeof(m_debug_settings.imei) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

const char * lwm2m_debug_msisdn_get(void)
{
    return m_debug_settings.msisdn;
}

int32_t lwm2m_debug_msisdn_set(const char * msisdn)
{
    memset(m_debug_settings.msisdn, 0, sizeof(m_debug_settings.msisdn));
    strncpy(m_debug_settings.msisdn, msisdn, sizeof(m_debug_settings.msisdn) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

const char * lwm2m_debug_modem_logging_get(void)
{
    return m_debug_settings.modem_logging;
}

int32_t lwm2m_debug_modem_logging_set(const char * modem_logging)
{
    memset(m_debug_settings.modem_logging, 0, sizeof(m_debug_settings.modem_logging));
    strncpy(m_debug_settings.modem_logging, modem_logging, sizeof(m_debug_settings.modem_logging) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

