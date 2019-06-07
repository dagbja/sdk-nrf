/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdint.h>

#define LOG_MODULE_NAME lwm2m_sms

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <sms_receive.h>
#include <at_cmd.h>
#include <lwm2m_vzw_main.h>

static uint32_t receive_count = 0;

int32_t sms_receiver_init(void)
{
    LOG_INF("Initializing SMS receiver.");

    // Selects how new messages are indicated.
    int err = at_cmd_write("AT+CNMI=3,2,0,1", NULL, 0, NULL);

    if (err) {
        LOG_ERR("Unable to initializing SMS receiver AT error %d.", err);
        return err;
    }

    return 0;
}

void sms_receiver_notif_parse(char *notif)
{
    // Check if this is an SMS notification.
    int length = strlen(notif);

    if (length > 12 && strncmp(notif, "+CMT:", 5) == 0) {

        receive_count++;
        //LOG_INF("SMS received (count=%lu)", receive_count);

        // Send new message ACK in PDU mode.
        int err = at_cmd_write("AT+CNMA=1", NULL, 0, NULL);
        if(err != 0) {
            // Ignore error and continue
            LOG_ERR("Unable to ACK SMS notification.");
        }

        // Manually decode the last bytes to get CoAP URI (ignore trailing \r\n)
        uint8_t object = notif[length-11] - '0';
        uint8_t instance = notif[length-7] - '0';
        uint8_t resource = notif[length-3] - '0';

        if (object == 1 && instance >= 0 && instance < 4 && resource == 8) {
            // Server Registration Update Trigger
            LOG_INF("Server Registration Update Trigger (server %u)", instance);
            lwm2m_request_server_update(instance, false);
        } else if (object == 3 && instance == 0 && resource == 4) {
            // Device Reboot
            LOG_INF("Device Reboot");
            lwm2m_system_reset();
        } else if (object == 3 && instance == 0 && resource == 5) {
            // Device Factory Reset
            LOG_INF("Device Factory Reset");
            lwm2m_factory_reset();
            lwm2m_system_reset();
        } else {
            LOG_ERR("Execute /%d/%d/%d not handled", object, instance, resource);
        }
    }
}

uint32_t sms_receive_counter(void)
{
    return receive_count;
}
