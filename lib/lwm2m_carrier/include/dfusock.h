/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef DFUSOCK_H__
#define DFUSOCK_H__

#include <stddef.h>
#include <stdint.h>
#include <nrf_socket.h>

int dfusock_init(void);
int dfusock_close(void);
int dfusock_offset_get(uint32_t *off);
int dfusock_offset_set(uint32_t off);

int dfusock_flash_size_get(uint32_t *size);

/**@brief Retrive the modem firmware version.
 *
 * If the buffer is large enough, additionally NULL-terminates
 * the buffer so that it can be printed.
 */
int dfusock_version_get(uint8_t *buf, size_t len);

int dfusock_fragment_send(const void *buf, size_t len);
int dfusock_firmware_update(void);
int dfusock_firmware_delete(void);

int dfusock_error_get(nrf_dfu_err_t *err);

#endif // DFUSOCK_H__
