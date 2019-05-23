/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#if CONFIG_DK_LIBRARY
#include <buttons_and_leds.h>
#endif

#include <app_debug.h>
#include <lwm2m_vzw_main.h>
#include <sms_receive.h>

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

/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t error)
{
#if CONFIG_DK_LIBRARY
    ARG_UNUSED(error);
    buttons_and_leds_uninit();
#endif
    printk("IRRECOVERABLE ERROR %lu\n", error);
    while (true);
}

/**@brief Function for application main entry.
 */
int main(void)
{
    printk("Initializing LTE link\n");

    int err = lwm2m_vzw_init();
    __ASSERT(err == 0, "Failed to initialize VZW LWM2M");

#if CONFIG_DK_LIBRARY
    // Initialize LEDs and Buttons.
    buttons_and_leds_init();
#endif

    if (app_debug_flag_is_set(DEBUG_FLAG_SMS_SUPPORT)) {
        // Start SMS receive thread
        sms_receive_thread_start();
    }

    // Non-return function.
    lwm2m_vzw_run();

    return 0;
}
