/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>

int lwm2m_firmware_download_init(void);
int lwm2m_firmware_download_uri(char *uri, size_t len);
int lwm2m_firmware_download_apply(void);
