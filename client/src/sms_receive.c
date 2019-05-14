/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LOG_MODULE_NAME lwm2m_sms

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr.h>
#include <stdio.h>
#include <stdint.h>
#include <net/socket.h>
#include <main.h>

#define APP_MAX_AT_READ_LENGTH          256
#define APP_MAX_AT_WRITE_LENGTH         256

static K_THREAD_STACK_ARRAY_DEFINE(sms_stack, 1, 1024);
static struct k_thread sms_thread;
static uint32_t receive_count = 0;

static void sms_receive(void *id, void *unused1, void *unused2)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        LOG_ERR("socket() failed");
        return;
    }

    const char *at_cnmi = "AT+CNMI=3,2,0,1";  // Selects how new messages are indicated
    const char *at_cnma = "AT+CNMA=1";        // Send new message ACK in PDU mode

    LOG_INF("Initializing SMS receiver");
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_cnmi);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    while (true) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);

        if (length > 12 && strncmp(read_buffer, "+CMT:", 5) == 0) {
            // Send ACK
            send(at_socket_fd, at_cnma, strlen(at_cnma), 0);
            receive_count++;

            // Manually decode the last bytes to get CoAP URI (ignore trailing \r\n)
            uint8_t object  = read_buffer[length-12] - '0';
            uint8_t instance = read_buffer[length-8] - '0';
            uint8_t resource = read_buffer[length-4] - '0';

            if (object == 1 && instance >= 0 && instance < 4 && resource == 8) {
                // Server Registration Update Trigger
                LOG_INF("Server Registration Update Trigger (server %u)", instance);
                app_request_server_update(instance);
            } else if (object == 3 && instance == 0 && resource == 4) {
                // Device Reboot
                LOG_INF("Device Reboot");
                app_request_reboot();
            } else if (object == 3 && instance == 0 && resource == 5) {
                // Device Factory Reset
                LOG_INF("Device Factory Reset");
                app_factory_reset();
                app_system_reset();
            } else {
                LOG_ERR("Execute /%d/%d/%d not handled", object, instance, resource);
            }
        }
    }

    LOG_ERR("exit");
    close(at_socket_fd);
}

uint32_t sms_receive_counter(void)
{
    return receive_count;
}

void sms_receive_thread_start(void)
{
    static bool initialized;

    if (!initialized) {
        k_thread_create(&sms_thread, sms_stack[0], 1024,
                        sms_receive, NULL, NULL, NULL, 0,
                        K_USER, K_FOREVER);
        k_thread_start(&sms_thread);

        initialized = true;
    }
}