/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <lwm2m.h>

#if CONFIG_DK_LIBRARY
#include <client_leds.h>
#endif

#include <lwm2m_carrier.h>
#include <modem_logging.h>

int lwm2m_carrier_event_handler(const lwm2m_carrier_event_t * event)
{
    lwm2m_carrier_event_error_t *error_event;
    lwm2m_carrier_event_deferred_t *deferred_event;

    static const char *str[] = {
        [LWM2M_CARRIER_EVENT_BSDLIB_INIT] = "LWM2M_CARRIER_EVENT_BSDLIB_INIT",
        [LWM2M_CARRIER_EVENT_CONNECTING] = "LWM2M_CARRIER_EVENT_CONNECTING",
        [LWM2M_CARRIER_EVENT_CONNECTED] = "LWM2M_CARRIER_EVENT_CONNECTED",
        [LWM2M_CARRIER_EVENT_DISCONNECTING] = "LWM2M_CARRIER_EVENT_DISCONNECTING",
        [LWM2M_CARRIER_EVENT_DISCONNECTED] = "LWM2M_CARRIER_EVENT_DISCONNECTED",
        [LWM2M_CARRIER_EVENT_BOOTSTRAPPED] = "LWM2M_CARRIER_EVENT_BOOTSTRAPPED",
        [LWM2M_CARRIER_EVENT_REGISTERED] = "LWM2M_CARRIER_EVENT_REGISTERED",
        [LWM2M_CARRIER_EVENT_DEFERRED] = "LWM2M_CARRIER_EVENT_DEFERRED",
        [LWM2M_CARRIER_EVENT_FOTA_START] = "LWM2M_CARRIER_EVENT_FOTA_START",
        [LWM2M_CARRIER_EVENT_REBOOT] = "LWM2M_CARRIER_EVENT_REBOOT",
        [LWM2M_CARRIER_EVENT_LTE_READY] = "LWM2M_CARRIER_EVENT_LTE_READY",
        [LWM2M_CARRIER_EVENT_ERROR] = "LWM2M_CARRIER_EVENT_ERROR",
    };

    switch (event->type) {
    case LWM2M_CARRIER_EVENT_BSDLIB_INIT:
    case LWM2M_CARRIER_EVENT_CONNECTING:
    case LWM2M_CARRIER_EVENT_CONNECTED:
    case LWM2M_CARRIER_EVENT_DISCONNECTING:
    case LWM2M_CARRIER_EVENT_DISCONNECTED:
    case LWM2M_CARRIER_EVENT_BOOTSTRAPPED:
    case LWM2M_CARRIER_EVENT_REGISTERED:
    case LWM2M_CARRIER_EVENT_FOTA_START:
    case LWM2M_CARRIER_EVENT_REBOOT:
    case LWM2M_CARRIER_EVENT_LTE_READY:
        LWM2M_INF("Sent event %s", lwm2m_os_log_strdup(str[event->type]));
        break;
    case LWM2M_CARRIER_EVENT_DEFERRED:
        deferred_event = (lwm2m_carrier_event_deferred_t *)(event->data);
        LWM2M_INF("Sent event %s: [%u] %ds", lwm2m_os_log_strdup(str[event->type]),
                  deferred_event->reason, deferred_event->timeout);
        break;
    case LWM2M_CARRIER_EVENT_ERROR:
        error_event = (lwm2m_carrier_event_error_t *)(event->data);
        LWM2M_INF("Sent event %s: [%u] %d", lwm2m_os_log_strdup(str[event->type]),
                  error_event->code, error_event->value);
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
    client_leds_init();
#endif

    printk("\n*** LwM2M client build %s (built on %s) ***",
           STRINGIFY(LWM2M_CLIENT_VERSION), STRINGIFY(LWM2M_CLIENT_BUILD_HOST));

    return 0;
}

#if !defined(CONFIG_LWM2M_CARRIER)
/* LWM2M background thread - should become a separate module. */

/* These should be configurable. */
#define LWM2M_CARRIER_THREAD_STACK_SIZE 4096
#define LWLM2_CARRIER_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

void lwm2m_carrier_thread_run(void)
{
    // Configure FIDO trace first to ensure we capture lwm2m_carrier_init().
    modem_trace_enable();

    int err = lwm2m_carrier_init(NULL);
    if (err != 0) {
        printk("Failed to initialize the LWM2M carrier library (%d). Exit!\n", err);
        __ASSERT(false, "Failed to initialize the LWM2M carrier library (%d). Exit!\n", err);
        return;
    }

    // Initialize logging.
    modem_logging_init();

    // Non-return function.
    lwm2m_carrier_run();
}

K_THREAD_DEFINE(lwm2m_carrier_thread, LWM2M_CARRIER_THREAD_STACK_SIZE,
                lwm2m_carrier_thread_run, NULL, NULL, NULL,
                LWLM2_CARRIER_THREAD_PRIORITY, 0, NO_WAIT);
#endif
