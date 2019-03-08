/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <net/socket.h>

#define APP_MAX_AT_READ_LENGTH          256
#define APP_MAX_AT_WRITE_LENGTH         256

char imei[16];
char msisdn[16];

void at_read_imei_and_msisdn(void)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return;
    }

    // Read IMEI
    const char *at_cgsn = "AT+CGSN";
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_cgsn);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        memset(imei, 0, sizeof(imei));
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            memcpy(imei, read_buffer, 15);
        } else {
            printk("recv(%s) failed\n", at_cgsn);
        }
    } else {
        printk("send(%s) failed\n", at_cgsn);
    }

    // Read MSISDN
    const char *at_cnum = "AT+CNUM";
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_cnum);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        memset(msisdn, 0, sizeof(msisdn));
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            char * p_start = strstr(read_buffer, "\"");
            if (p_start) {
                char * p_end = strstr(p_start + 1, "\"");
                if (p_end && (p_end - p_start - 1 >= 10)) {
                    // FIXME: This only uses the last 10 digits
                    memcpy(msisdn, p_end - 10, 10);
                }
            }
            if (!msisdn[0]) {
                // SIM has no number
                memcpy(msisdn, "0000000000", 10);
            }
        } else {
            printk("recv(%s) failed\n", at_cnum);
        }
    } else {
        printk("send(%s) failed\n", at_cnum);
    }

    close(at_socket_fd);
}


void at_send_command(const char *at_command, bool do_logging)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return;
    }

    if (do_logging) {
        printk("send: %s\n", at_command);
    }

    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_command);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            if (do_logging) {
                printk("recv: %s\n", read_buffer);
            }
        } else {
            printk("recv() failed\n");
        }
    } else {
        printk("send() failed\n");
    }

    close(at_socket_fd);
}