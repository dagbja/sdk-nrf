/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <lwm2m_objects.h>
#include <coap_message.h>
#include <coap_block.h>

/* Shorter definitions for firmware resource values */

#define STATE_IDLE 		LWM2M_FIRMWARE_STATE_IDLE
#define STATE_DOWNLOADING 	LWM2M_FIRMWARE_STATE_DOWNLOADING
#define STATE_DOWNLOADED 	LWM2M_FIRMWARE_STATE_DOWNLOADED
#define STATE_UPDATING 		LWM2M_FIRMWARE_STATE_UPDATING

#define RESULT_DEFAULT			LWM2M_FIRMWARE_UPDATE_RESULT_DEFAULT
#define RESULT_SUCCESS			LWM2M_FIRMWARE_UPDATE_RESULT_SUCCESS
#define RESULT_ERROR_STORAGE		LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_STORAGE
#define RESULT_ERROR_MEMORY		LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_MEMORY
#define RESULT_ERROR_CONN_LOST		LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CONN_LOST
#define RESULT_ERROR_CRC		LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC
#define RESULT_ERROR_UNSUP_PKG_TYPE	LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PKG_TYPE
#define RESULT_ERROR_INVALID_URI	LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI
#define RESULT_ERROR_UPDATE_FAILED	LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_FIRMWARE_UPDATE_FAILED
#define RESULT_ERROR_UNSUP_PROTO	LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PROTOCOL

int lwm2m_firmware_download_init(void);
int lwm2m_firmware_download_resume(void);
int lwm2m_firmware_download_uri(char *uri, size_t len);
int lwm2m_firmware_download_apply(void);
void lwm2m_firmware_download_reboot_schedule(int timeout);

int lwm2m_firmware_download_inband(const coap_message_t *req, coap_message_t *rsp,
				   coap_block_opt_block1_t *block1);
