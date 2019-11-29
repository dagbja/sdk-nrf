/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <lwm2m.h>

#if CONFIG_DK_LIBRARY
#include <buttons_and_leds.h>
#endif

#include <lwm2m_carrier.h>
#include <modem_logging.h>

int lwm2m_carrier_event_handler(const lwm2m_carrier_event_t * event)
{
    ARG_UNUSED(event);

    static const char *str[] = {
        [LWM2M_CARRIER_EVENT_BSDLIB_INIT] = "LWM2M_CARRIER_EVENT_BSDLIB_INIT",
        [LWM2M_CARRIER_EVENT_CONNECTING] = "LWM2M_CARRIER_EVENT_CONNECTING",
        [LWM2M_CARRIER_EVENT_CONNECTED] = "LWM2M_CARRIER_EVENT_CONNECTED",
        [LWM2M_CARRIER_EVENT_DISCONNECTING] = "LWM2M_CARRIER_EVENT_DISCONNECTING",
        [LWM2M_CARRIER_EVENT_DISCONNECTED] = "LWM2M_CARRIER_EVENT_DISCONNECTED",
        [LWM2M_CARRIER_EVENT_BOOTSTRAPPED] = "LWM2M_CARRIER_EVENT_BOOTSTRAPPED",
        [LWM2M_CARRIER_EVENT_READY] = "LWM2M_CARRIER_EVENT_READY",
        [LWM2M_CARRIER_EVENT_FOTA_START] = "LWM2M_CARRIER_EVENT_FOTA_START",
        [LWM2M_CARRIER_EVENT_REBOOT] = "LWM2M_CARRIER_EVENT_REBOOT",
    };

    switch (event->type) {
    case LWM2M_CARRIER_EVENT_BSDLIB_INIT:
    case LWM2M_CARRIER_EVENT_CONNECTING:
    case LWM2M_CARRIER_EVENT_CONNECTED:
    case LWM2M_CARRIER_EVENT_DISCONNECTING:
    case LWM2M_CARRIER_EVENT_DISCONNECTED:
    case LWM2M_CARRIER_EVENT_BOOTSTRAPPED:
    case LWM2M_CARRIER_EVENT_READY:
    case LWM2M_CARRIER_EVENT_FOTA_START:
    case LWM2M_CARRIER_EVENT_REBOOT:
        LWM2M_INF("Sent event %s", lwm2m_os_log_strdup(str[event->type]));
        break;
    default:
        /* TODO: assert */
        break;
    }

    return 0;
}

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t error)
{
#if CONFIG_DK_LIBRARY
    ARG_UNUSED(error);
    leds_recoverable_error_loop();
#else
    printk("RECOVERABLE ERROR %lu\n", error);
    while (true);
#endif
}

/**@brief Function for application main entry.
 */
int main(void)
{
#if CONFIG_DK_LIBRARY
    // Initialize LEDs and Buttons.
    buttons_and_leds_init();
#endif

    return 0;
}

#if !defined(CONFIG_LWM2M_CARRIER)
/* LWM2M background thread - should become a separate module. */

/* These should be configurable. */
#define LWM2M_VZW_THREAD_STACK_SIZE 8192
#define LWLM2_VZW_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

void lwm2m_vzw_thread_run(void)
{
    // Start FIDO trace first to ensure we capture lwm2m_carrier_init().
    modem_trace_enable();

    const lwm2m_carrier_config_t carrier_config = {
        .bootstrap_uri = "coaps://xvzwcdpii.xdev.motive.com:5684"
    };

    int err = lwm2m_carrier_init(&carrier_config);
    if (err != 0) {
        printk("Failed to initialize the VZW LWM2M carrier library (%d). Exit!\n", err);
        __ASSERT(false, "Failed to initialize the VZW LWM2M carrier library (%d). Exit!\n", err);
        return;
    }

    // Initialize logging.
    modem_logging_init();

    // Non-return function.
    lwm2m_carrier_run();
}

K_THREAD_DEFINE(lwm2m_vzw_thread, LWM2M_VZW_THREAD_STACK_SIZE,
                lwm2m_vzw_thread_run, NULL, NULL, NULL,
                LWLM2_VZW_THREAD_PRIORITY, 0, K_NO_WAIT);
#endif
