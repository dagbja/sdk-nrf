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
#include <sms_receive.h>
#include <lwm2m_vzw_main.h>
#include <lwm2m_carrier.h>

K_SEM_DEFINE(lwm2m_vzw_sem, 0, 1);

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
#if CONFIG_DK_LIBRARY
    // Initialize LEDs and Buttons.
    buttons_and_leds_init();
#endif

    k_sem_take(&lwm2m_vzw_sem, K_FOREVER);

    if (app_debug_flag_is_set(DEBUG_FLAG_SMS_SUPPORT)) {
        //Enable SMS.
        sms_receiver_init();
    }

    for (;;) {
        k_sleep(K_SECONDS(1));
    }
}

/* LWM2M background thread - should become a separate module. */

/* These should be configurable. */
#define LWM2M_VZW_THREAD_STACK_SIZE 8192
#define LWLM2_VZW_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO

void lwm2m_vzw_thread_run(void)
{
    int err = lwm2m_carrier_init();
    __ASSERT(err == 0, "Failed to initialize VZW LWM2M");

    if(err != 0) {
        return;
    }

    k_sem_give(&lwm2m_vzw_sem);

    // Non-return function.
    lwm2m_carrier_run();
}

K_THREAD_DEFINE(lwm2m_vzw_thread, LWM2M_VZW_THREAD_STACK_SIZE,
                lwm2m_vzw_thread_run, NULL, NULL, NULL,
                LWLM2_VZW_THREAD_PRIORITY, 0, K_NO_WAIT);
