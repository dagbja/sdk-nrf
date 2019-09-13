/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <at_cmd.h>
#include <at_interface.h>
#include <lwm2m_os.h>
#include <modem_logging.h>

#define MODEM_LOGGING_STORAGE_ID 0x4242
#define APP_MAX_AT_READ_LENGTH   CONFIG_AT_CMD_RESPONSE_MAX_LEN

static char modem_logging[65];

int modem_logging_init(void)
{
    int ret = lwm2m_os_storage_read(MODEM_LOGGING_STORAGE_ID, modem_logging, sizeof(modem_logging));

    modem_logging_enable();

    return ret;
}

const char * modem_logging_get(void)
{
    return modem_logging;
}

int32_t modem_logging_set(const char * new_modem_logging)
{
    memset(modem_logging, 0, sizeof(modem_logging));
    strncpy(modem_logging, new_modem_logging, sizeof(modem_logging) - 1);

    return lwm2m_os_storage_write(MODEM_LOGGING_STORAGE_ID, modem_logging, sizeof(modem_logging));
}

int modem_at_write(const char *const cmd, bool do_logging)
{
    int ret = 0;
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    if (cmd == NULL) {
        ret = -1;
    } else {
        // Send a null-terminated AT command.
        ret = at_cmd_write(cmd, read_buffer, APP_MAX_AT_READ_LENGTH, NULL);
    }

    if (do_logging) {
        if (ret == 0) {
            printk("%s", read_buffer);
        } else {
            // Unable to send the AT command or received an error response.
            // This usually indicates that an error has been received in the AT response.
            printk("AT error %d", ret);
        }
    }

    return ret;
}


void modem_trace_enable(void)
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


// fidoless modem trace options:
//   1,0 = disable
//   1,1 = coredump only
//   1,2 = generic (and coredump)
//   1,3 = lwm2m   (and coredump)
//   1,4 = ip only (and coredump)
void modem_logging_enable(void)
{
    if ((modem_logging[0] == 0) ||
        (strcmp(modem_logging, "0") == 0)) {
        modem_at_write("AT%XMODEMTRACE=1,0", false);
    } else if (strcmp(modem_logging, "1") == 0) {
        modem_at_write("AT%XMODEMTRACE=1,2", false);
    } else if (strcmp(modem_logging, "2") == 0) {
        modem_at_write("AT%XMODEMTRACE=1,1", false);
    } else if (strcmp(modem_logging, "3") == 0) {
        modem_at_write("AT%XMODEMTRACE=1,3", false);
    } else if (strcmp(modem_logging, "4") == 0) {
        modem_at_write("AT%XMODEMTRACE=1,4", false);
    } else if (strlen(modem_logging) == 64) {
        char at_command[128];
        sprintf(at_command, "AT%%XMODEMTRACE=2,,3,%s", modem_logging);
        modem_at_write(at_command, false);
    }
}
