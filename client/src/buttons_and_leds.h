/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BUTTONS_AND_LEDS_H__
#define BUTTONS_AND_LEDS_H__

void buttons_and_leds_init(void);
void buttons_and_leds_uninit(void);

void leds_recoverable_error_loop(void);
void leds_error_loop(void);

#endif // BUTTONS_AND_LEDS_H__