/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_debug

#include <zephyr.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <lwm2m_instance_storage.h>
#include <app_debug.h>
#include <at_interface.h>


static debug_settings_t m_debug_settings;

void app_debug_init(void)
{
    lwm2m_debug_settings_load(&m_debug_settings);
}

void app_debug_clear(void)
{
    memset(&m_debug_settings, 0, sizeof(m_debug_settings));
    lwm2m_debug_settings_store(&m_debug_settings);
}

const char * app_debug_imei_get(void)
{
    return m_debug_settings.imei;
}

int32_t app_debug_imei_set(const char * imei)
{
    memset(m_debug_settings.imei, 0, sizeof(m_debug_settings.imei));
    strncpy(m_debug_settings.imei, imei, sizeof(m_debug_settings.imei) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

const char * app_debug_msisdn_get(void)
{
    return m_debug_settings.msisdn;
}

int32_t app_debug_msisdn_set(const char * msisdn)
{
    memset(m_debug_settings.msisdn, 0, sizeof(m_debug_settings.msisdn));
    strncpy(m_debug_settings.msisdn, msisdn, sizeof(m_debug_settings.msisdn) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

const char * app_debug_modem_logging_get(void)
{
    return m_debug_settings.modem_logging;
}

int32_t app_debug_modem_logging_set(const char * modem_logging)
{
    memset(m_debug_settings.modem_logging, 0, sizeof(m_debug_settings.modem_logging));
    strncpy(m_debug_settings.modem_logging, modem_logging, sizeof(m_debug_settings.modem_logging) - 1);

    return lwm2m_debug_settings_store(&m_debug_settings);
}

static void modem_trace_enable(void)
{
    /* GPIO configurations for trace and debug */
    #define CS_PIN_CFG_TRACE_CLK    21 //GPIO_OUT_PIN21_Pos
    #define CS_PIN_CFG_TRACE_DATA0  22 //GPIO_OUT_PIN22_Pos
    #define CS_PIN_CFG_TRACE_DATA1  23 //GPIO_OUT_PIN23_Pos
    #define CS_PIN_CFG_TRACE_DATA2  24 //GPIO_OUT_PIN24_Pos
    #define CS_PIN_CFG_TRACE_DATA3  25 //GPIO_OUT_PIN25_Pos

    // Configure outputs.
    // CS_PIN_CFG_TRACE_CLK
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_CLK] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                               (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA0
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA0] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA1
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA1] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA2
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA2] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    // CS_PIN_CFG_TRACE_DATA3
    NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA3] = (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                                 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

    NRF_P0_NS->DIR = 0xFFFFFFFF;
}


void app_debug_modem_logging_enable(bool modem_initialized)
{
    if (modem_initialized) {
        if (strcmp(m_debug_settings.modem_logging, "1") == 0) {
            // 1,0 = disable
            // 1,1 = coredump only
            // 1,2 = generic (and coredump)
            // 1,3 = lwm2m   (and coredump)
            // 1,4 = ip only (and coredump)
            at_send_command("AT%XMODEMTRACE=1,2", false);
            at_send_command("AT%XMODEMTRACE=1,3", false);
            at_send_command("AT%XMODEMTRACE=1,4", false);
        } else if (strlen(m_debug_settings.modem_logging) == 64) {
            char at_command[128];
            sprintf(at_command, "AT%%XMODEMTRACE=2,,3,%s", m_debug_settings.modem_logging);
            at_send_command(at_command, false);
        }
    } else {
        if (strcmp(m_debug_settings.modem_logging, "2") == 0) {
            modem_trace_enable();
        }
    }
}