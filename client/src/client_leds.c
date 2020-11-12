/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#if CONFIG_DK_LIBRARY

#include <zephyr.h>
#include <dk_buttons_and_leds.h>
#include <lwm2m_security.h>
#include <lwm2m_carrier_main.h>

/**< Interval in milliseconds between each time status LEDs are updated. */
#define APP_LEDS_UPDATE_INTERVAL K_MSEC(500)

/* Structures for delayed work */
static struct k_delayed_work leds_update_work;

static void app_leds_get_state(uint8_t *on, uint8_t *blink)
{
    *on = 0;
    *blink = 0;

    switch (lwm2m_state_get())
    {
        case LWM2M_STATE_BOOTING:
            *blink = DK_LED1_MSK;
            break;

        case LWM2M_STATE_DISCONNECTED:
            *on = DK_LED1_MSK;
            break;

        case LWM2M_STATE_IDLE:
            *on = (DK_LED1_MSK | DK_LED3_MSK);
            break;

        case LWM2M_STATE_REQUEST_LINK_UP:
        case LWM2M_STATE_REQUEST_LINK_DOWN:
        case LWM2M_STATE_REQUEST_CONNECT:
        case LWM2M_STATE_REQUEST_DISCONNECT:
            *on = DK_LED3_MSK;
            *blink = DK_LED1_MSK;
            break;

        case LWM2M_STATE_MODEM_FIRMWARE_UPDATE:
            *blink = DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK;
            break;

        case LWM2M_STATE_SHUTDOWN:
        case LWM2M_STATE_RESET:
            *blink = DK_LED4_MSK;
            break;

        case LWM2M_STATE_ERROR:
            *blink = DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK;
            break;
    }
}

/**@brief Update LEDs state. */
static void app_leds_update(struct k_work *work)
{
        static bool led_on;
        static uint8_t current_led_on_mask;
        uint8_t led_on_mask, led_blink_mask;

        ARG_UNUSED(work);

        /* Set led_on_mask to match current state. */
        app_leds_get_state(&led_on_mask, &led_blink_mask);

        if (lwm2m_did_bootstrap())
        {
            /* Only turn on LED2 if bootstrap was done. */
            led_on_mask |= DK_LED2_MSK;
        }

        led_on = !led_on;
        if (led_on) {
                led_on_mask |= led_blink_mask;
                if (led_blink_mask == 0) {
                    // Only blink LED4 if no other led is blinking
                    led_on_mask |= DK_LED4_MSK;
                }
        } else {
                led_on_mask &= ~led_blink_mask;
                led_on_mask &= ~DK_LED4_MSK;
        }

        if (led_on_mask != current_led_on_mask) {
                dk_set_leds(led_on_mask);
                current_led_on_mask = led_on_mask;
        }

        k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
}

void leds_recoverable_error_loop(void)
{
    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF in pairs (1 and 3, 2 and 4) if there is an recoverable error. */
    while (true) {
        dk_set_leds_state(DK_LED1_MSK | DK_LED3_MSK, DK_LED2_MSK | DK_LED4_MSK);
        k_sleep(K_MSEC(250));
        dk_set_leds_state(DK_LED2_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED3_MSK);
        k_sleep(K_MSEC(250));
    }
}

/**@brief Initializes LEDs, using the DK LEDs library. */
void client_leds_init(void)
{
    dk_leds_init();
    dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);

    k_delayed_work_init(&leds_update_work, app_leds_update);
    k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);
}

#endif // CONFIG_DK_LIBRARY
