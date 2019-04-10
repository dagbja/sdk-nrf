/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#if CONFIG_DK_LIBRARY

#include <zephyr.h>
#include <dk_buttons_and_leds.h>
#include <lwm2m_security.h>
#include <main.h>

/**< Interval in milliseconds between each time status LEDs are updated. */
#define APP_LEDS_UPDATE_INTERVAL 500

/* Structures for delayed work */
static struct k_delayed_work leds_update_work;


/**@brief Callback for button events from the DK buttons and LEDs library. */
static void app_button_handler(u32_t buttons, u32_t has_changed)
{
    app_state_t app_state = app_state_get();

    if (buttons & 0x01) // Button 1 has changed
    {
        if (!(buttons & 0x08)) // Switch 2 in right position
        {
            printk("Factory reset!\n");
            app_factory_reset();
            app_system_reset();
        }
        else if (app_state == APP_STATE_IP_INTERFACE_UP)
        {
            if (lwm2m_security_bootstrapped_get(0))
            {
                app_state_set(APP_STATE_SERVER_CONNECT);
            }
            else
            {
                app_state_set(APP_STATE_BS_CONNECT);
            }
        }
        else if (app_state == APP_STATE_SERVER_REGISTERED)
        {
            app_update_server(1);
        }
    }
    else if (buttons & 0x02) // Button 2 has changed
    {
        if (!(buttons & 0x08)) // Switch 2 in right position
        {
            printk("System shutdown!\n");
            app_system_shutdown();
        }
        else if (app_state == APP_STATE_SERVER_REGISTERED)
        {
            app_state = APP_STATE_SERVER_DEREGISTER;
        }
        else if (app_state == APP_STATE_IP_INTERFACE_UP)
        {
            app_system_reset();
        }
    }
}

static void app_leds_get_state(u8_t *on, u8_t *blink)
{
    *on = 0;
    *blink = 0;

    switch (app_state_get())
    {
        case APP_STATE_IDLE:
            *blink = DK_LED1_MSK;
            break;

        case APP_STATE_IP_INTERFACE_UP:
            *on = DK_LED1_MSK;
            break;

        case APP_STATE_BS_CONNECT:
        case APP_STATE_BS_CONNECT_WAIT:
            *blink = (DK_LED1_MSK | DK_LED2_MSK);
            break;

        case APP_STATE_BS_CONNECT_RETRY_WAIT:
            *blink = (DK_LED2_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_BS_CONNECTED:
        case APP_STATE_BOOTSTRAP_REQUESTED:
            *on = DK_LED1_MSK;
            *blink = DK_LED2_MSK;
            break;

        case APP_STATE_BOOTSTRAP_WAIT:
            *on = DK_LED1_MSK;
            *blink = (DK_LED2_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_BOOTSTRAPPING:
        case APP_STATE_BOOTSTRAP_TIMEDOUT:
            *on = (DK_LED1_MSK | DK_LED2_MSK);
            *blink = DK_LED4_MSK;
            break;

        case APP_STATE_BOOTSTRAPPED:
            *on = (DK_LED1_MSK | DK_LED2_MSK);
            break;

        case APP_STATE_SERVER_CONNECT:
        case APP_STATE_SERVER_CONNECT_WAIT:
            *blink = (DK_LED1_MSK | DK_LED3_MSK);
            break;

        case APP_STATE_SERVER_CONNECT_RETRY_WAIT:
            *blink = (DK_LED3_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_SERVER_CONNECTED:
            *on = DK_LED1_MSK;
            *blink = DK_LED3_MSK;
            break;

        case APP_STATE_SERVER_REGISTER_WAIT:
            *on = DK_LED1_MSK;
            *blink = (DK_LED3_MSK | DK_LED4_MSK);
            break;

        case APP_STATE_SERVER_REGISTERED:
            *on = (DK_LED1_MSK | DK_LED3_MSK);
            break;

        case APP_STATE_SERVER_DEREGISTER:
        case APP_STATE_SERVER_DEREGISTERING:
        case APP_STATE_DISCONNECT:
            *on = DK_LED3_MSK;
            *blink = DK_LED1_MSK;
            break;

        case APP_STATE_SHUTDOWN:
            *blink = DK_LED4_MSK;
            break;
    }
}

/**@brief Update LEDs state. */
static void app_leds_update(struct k_work *work)
{
        static bool led_on;
        static u8_t current_led_on_mask;
        u8_t led_on_mask, led_blink_mask;

        ARG_UNUSED(work);

        /* Set led_on_mask to match current state. */
        app_leds_get_state(&led_on_mask, &led_blink_mask);

        if (app_did_bootstrap())
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

/**@brief Check buttons pressed at startup. */
static void check_buttons_pressed(void)
{
    u32_t button_state = 0;
    dk_read_buttons(&button_state, NULL);

    // Check if button 1 pressed during startup
    if (button_state & 0x01) {
        app_factory_reset();

        printk("Factory reset!\n");
        k_delayed_work_cancel(&leds_update_work);
        while (true) { // Blink all LEDs
            dk_set_leds_state(DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK, 0);
            k_sleep(250);
            dk_set_leds_state(0, DK_LED1_MSK | DK_LED2_MSK | DK_LED3_MSK | DK_LED4_MSK);
            k_sleep(250);
        }
    }
}

void leds_error_loop(void)
{
    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF in pairs (1 and 2, 3 and 4) if there is an error. */
    while (true) {
        dk_set_leds_state(DK_LED1_MSK | DK_LED2_MSK, DK_LED3_MSK | DK_LED4_MSK);
        k_sleep(250);
        dk_set_leds_state(DK_LED3_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED2_MSK);
        k_sleep(250);
    }
}

void leds_recoverable_error_loop(void)
{
    k_delayed_work_cancel(&leds_update_work);

    /* Blinking all LEDs ON/OFF in pairs (1 and 3, 2 and 4) if there is an recoverable error. */
    while (true) {
        dk_set_leds_state(DK_LED1_MSK | DK_LED3_MSK, DK_LED2_MSK | DK_LED4_MSK);
        k_sleep(250);
        dk_set_leds_state(DK_LED2_MSK | DK_LED4_MSK, DK_LED1_MSK | DK_LED3_MSK);
        k_sleep(250);
    }
}

/**@brief Initializes buttons and LEDs, using the DK buttons and LEDs library. */
void buttons_and_leds_init(void)
{
    dk_buttons_init(app_button_handler);
    dk_leds_init();
    dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);

    k_delayed_work_init(&leds_update_work, app_leds_update);
    k_delayed_work_submit(&leds_update_work, APP_LEDS_UPDATE_INTERVAL);

    check_buttons_pressed();
}

void buttons_and_leds_uninit(void)
{
     k_delayed_work_cancel(&leds_update_work);
}

#endif // CONFIG_DK_LIBRARY