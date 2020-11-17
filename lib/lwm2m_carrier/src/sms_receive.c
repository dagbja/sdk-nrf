/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stdint.h>

#include <lwm2m.h>
#include <sms_receive.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_carrier_client.h>

#define SMS_BOOTSTRAP_TEXT_MSG  "E2F79B3EA7CBC370"  // 7bit encoded "bootstrap" as HEX
#define SMS_BOOTSTRAP_TEXT_LEN  (sizeof(SMS_BOOTSTRAP_TEXT_MSG) - 1)

static bool sms_initialized;
static uint32_t receive_count;

int32_t lwm2m_sms_receiver_enable(void)
{
    if (!sms_initialized) {
        LWM2M_INF("Enable SMS receiver");

        // Selects how new messages are indicated.
        int err = lwm2m_os_at_cmd_write("AT+CNMI=3,2,0,1", NULL, 0);

        if (err) {
            LWM2M_ERR("Unable to enable SMS receiver, AT error %d", err);
            return err;
        }

        sms_initialized = true;
    }

    return 0;
}

int32_t lwm2m_sms_receiver_disable(void)
{
    if (sms_initialized) {
        LWM2M_INF("Disable SMS receiver");

        // Turn off SMS indication.
        int err = lwm2m_os_at_cmd_write("AT+CNMI=0", NULL, 0);

        if (err) {
            LWM2M_ERR("Unable to disable SMS receiver, AT error %d", err);
            return err;
        }

        sms_initialized = false;
    }

    return 0;
}

int sms_receiver_notif_parse(const char *notif)
{
    // Check if this is an SMS notification.
    int length = strlen(notif);
    if (length > 12 && strncmp(notif, "+CMT:", 5) == 0) {

        receive_count++;

        lwm2m_acknowledge_sms();

        // Manually decode the last bytes to get CoAP URI (ignore trailing \r\n)
        uint8_t object = notif[length-11] - '0';
        uint8_t instance = notif[length-7] - '0';
        uint8_t resource = notif[length-3] - '0';

        if (length > SMS_BOOTSTRAP_TEXT_LEN + 2) {
            const char *p_msg = &notif[length - SMS_BOOTSTRAP_TEXT_LEN - 2];
            if (memcmp(p_msg, SMS_BOOTSTRAP_TEXT_MSG, SMS_BOOTSTRAP_TEXT_LEN) == 0) {
                LWM2M_INF("SMS Text: Bootstrap Request Trigger");
                lwm2m_request_bootstrap();
                return 0;
            }
        }

        if (object == 1 && instance >= 0 && instance < 4 && resource == 8) {
            // Server Registration Update Trigger
            LWM2M_INF("SMS: Server Registration Update Trigger (instance %u)", instance);
            lwm2m_client_update(instance);
        } else if (object == 1 && instance == 0 && resource == 9) {
            // Bootstrap Request Trigger
            LWM2M_INF("SMS: Bootstrap Request Trigger");
            lwm2m_request_bootstrap();
        } else if (object == 3 && instance == 0 && resource == 4) {
            // Device Reboot
            LWM2M_INF("SMS: Device Reboot");
            lwm2m_request_reset();
        } else if (object == 3 && instance == 0 && resource == 5) {
            // Device Factory Reset
            LWM2M_INF("SMS: Device Factory Reset");
            lwm2m_factory_reset();
            lwm2m_request_reset();
        } else {
            LWM2M_ERR("SMS: Execute /%d/%d/%d not handled", object, instance, resource);
        }

        // CMT notification has been parsed.
        return 0;
    }

    // Not SMS related.
    return -1;
}

uint32_t lwm2m_sms_receive_counter(void)
{
    return receive_count;
}
