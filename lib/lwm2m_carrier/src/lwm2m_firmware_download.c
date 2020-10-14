/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <dfusock.h>
#include <nrf_socket.h>
#include <nrf_errno.h>

#include <lwm2m.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>
#include <lwm2m_carrier.h>
#include <lwm2m_firmware.h>
#include <lwm2m_firmware_download.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_device.h>

#include <coap_message.h>
#include <coap_block.h>

#include <operator_check.h>

/* The offset is set to this value for dirty images, or backup images */
#define DIRTY_IMAGE 2621440
/* Modem UUID string length, without NULL terminatation */
#define UUID_LEN sizeof(nrf_dfu_fw_version_t)
/* Modem UUID string length, including NULL termination */
#define PRINTABLE_UUID_LEN (sizeof(nrf_dfu_fw_version_t) + 1)
/* Byte-length, without NULL termination */
#define BYTELEN(string) (sizeof(string) - 1)
/* Interval at which to poll the offset of the scratch area
 * to determine if the erase operation has completed.
 */
#define OFFSET_POLL_INTERVAL SECONDS(2)
/* Interval at which to poll for network availability */
#define NETWORK_POLL_INTERVAL SECONDS(6)
/* Number of times to retry a download */
#define DOWNLOAD_RETRIES 8

#define NET_REG_OFFLINE 0

/* TODO: these are used by Pull-FOTA via CoAP only, not by Push-FOTA (inband).
 * They are working in Motive and AT&T test framework, but we should find
 * a way to fetch these at runtime, since they might change.
 */
#define VZW_DM_SEC_TAG 26
#define ATT_DM_SEC_TAG 27

static char apn[64];
static char package_url[URL_SIZE];
static uint32_t flash_size;
static bool check_file_size;

/* Number of times to retry a download on socket or HTTP errors.
 * This excludes the HTTP server closing the connection, since
 * that is retried automatically by the download_client.
 */
static uint8_t download_retries = DOWNLOAD_RETRIES;

static void *download_dwork;
static void *delete_dwork;
static void *reboot_dwork;
static struct lwm2m_os_download_cfg config = {
	.sec_tag = CONFIG_NRF_LWM2M_CARRIER_SEC_TAG,
};

static const char * const img_state_str[] = {
	[FIRMWARE_NONE] = "no image",
	[FIRMWARE_DOWNLOADING_PULL] = "downloading (pull)",
	[FIRMWARE_DOWNLOADING_PUSH] = "downloading (push)",
	[FIRMWARE_READY] = "complete image",
};

int lwm2m_firmware_download_uri(char *package_uri, size_t len);

static void carrier_evt_send(uint32_t type, void *data)
{
	lwm2m_carrier_event_t evt = {
		.type = type,
		.data = data,
	};

	lwm2m_carrier_event_handler(&evt);
}

static void carrier_error_evt_send(uint32_t id, int32_t err)
{
	/* There are five FOTA errors:
	 *
	 * _FOTA_FAIL
	 * The modem failed to update.
	 * The error is always zero.
	 *
	 * _FOTA_PKG
	 * The modem has rejected a package or refused to apply the update.
	 * The error is the dfu_err from the modem.
	 *
	 * _FOTA_PROTO
	 * The HTTP request failed (wrong URI, unexpected response).
	 * The error is an NCS error from the download_client.
	 *
	 * _FOTA_CONN
	 * Failed to connect the TCP socket.
	 * We could have failed to resolve the host IP addr,
	 * or it could have refused our connection (wrong cert).
	 *
	 * _FOTA_CONN_LOST
	 * Connection lost.
	 */
	lwm2m_carrier_event_t evt = {
		.type = LWM2M_CARRIER_EVENT_ERROR,
		.data = &(lwm2m_carrier_event_error_t) {
			.code = id,
			.value = err,
		}
	};

	lwm2m_carrier_event_handler(&evt);
}

static bool download_retry_and_update(void)
{
	if (download_retries == 0) {
		download_retries = DOWNLOAD_RETRIES;
		return false;
	}

	download_retries--;
	return true;
}

static bool file_size_check_valid(void)
{
	uint32_t file_size;

	check_file_size = false;
	lwm2m_os_download_file_size_get(&file_size);

	LWM2M_INF("File size: %d", file_size);

	return (file_size < flash_size);
}

int lwm2m_firmware_download_inband(const coap_message_t *req,
				   coap_message_t *rsp,
				   coap_block_opt_block1_t *block1)
{
	int err;
	const void *frag;
	size_t frag_len;
	size_t tlv_hdr_len;
	nrf_dfu_err_t dfu_err;

	static int last_blk = -1;

	if (block1->size != 512) {
		/* Enforce a smaller block size */
		block1->size = 512;
		rsp->header.code = COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE;
		return 0;
	}

	if (block1->number == last_blk) {
		/* This is a retransmission, don't pipe */
		LWM2M_WRN("Restransmission detected");
		rsp->header.code = COAP_CODE_231_CONTINUE;
		return 0;
	}

	last_blk = block1->number;

	if (block1->number == 0) {
		tlv_hdr_len = lwm2m_tlv_header_size_get(req->payload);

		frag = req->payload + tlv_hdr_len;
		frag_len = req->payload_len - tlv_hdr_len;

		/* Starting FOTA, reset result */
		lwm2m_firmware_update_result_set(0, RESULT_DEFAULT);
		lwm2m_firmware_state_set(0, STATE_DOWNLOADING);
		lwm2m_firmware_image_state_set(FIRMWARE_DOWNLOADING_PUSH);

		carrier_evt_send(LWM2M_CARRIER_EVENT_FOTA_START, NULL);

		err = dfusock_init();
		if (err) {
			return err;
		}
	} else {
		frag = req->payload;
		frag_len = req->payload_len;
	}

	err = dfusock_fragment_send(frag, frag_len);
	if (err) {
		dfusock_error_get(&dfu_err);
		dfusock_close();

		LWM2M_ERR("Reject reason : %d", (int)dfu_err);
		lwm2m_firmware_state_set(0, STATE_IDLE);
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_CRC);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_PKG, dfu_err);

		/* Reply a 4xx code to stop the server from sending more */
		rsp->header.code = COAP_CODE_400_BAD_REQUEST;

		/* Delete image now */
		lwm2m_os_timer_start(delete_dwork, NO_WAIT);
		return 0;
	}

	if (block1->more) {
		rsp->header.code = COAP_CODE_231_CONTINUE;
	} else {
		rsp->header.code = COAP_CODE_204_CHANGED;
		lwm2m_firmware_state_set(0, STATE_DOWNLOADED);
		lwm2m_firmware_image_state_set(FIRMWARE_READY);
	}

	return 0;
}

static int on_fragment(const struct lwm2m_os_download_evt *event)
{
	int err;
	nrf_dfu_err_t dfu_err = DFU_NO_ERROR;

	if (check_file_size && !file_size_check_valid()) {
		LWM2M_WRN("File size too large");
		lwm2m_os_download_disconnect();
		dfusock_close();

		lwm2m_firmware_state_set(0, STATE_IDLE);
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_STORAGE);
		/* Do not attempt to download again */
		lwm2m_firmware_image_state_set(FIRMWARE_NONE);

		/* Stop the download */
		return -1;
	}

	err = dfusock_fragment_send(event->fragment.buf, event->fragment.len);
	if (!err) {
		/* All good, continue the download */
		return 0;
	}

	/* The modem refused the fragment, give up */
	lwm2m_os_download_disconnect();
	dfusock_close();

	/* Report errors to server */
	lwm2m_firmware_state_set(0, STATE_IDLE);
	lwm2m_firmware_update_result_set(0, RESULT_ERROR_CRC);
	/* Do not attempt to download again */
	lwm2m_firmware_image_state_set(FIRMWARE_NONE);

	/* Report errors to user and delete firmware in flash */
	carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_PKG, dfu_err);
	lwm2m_os_timer_start(delete_dwork, NO_WAIT);

	/* Stop the download */
	return -1;
}

static int on_done(const struct lwm2m_os_download_evt *event)
{
	LWM2M_INF("Download completed");

	lwm2m_os_download_disconnect();
	dfusock_close();

	/* Save state and notify the server */
	lwm2m_firmware_image_state_set(FIRMWARE_READY);
	lwm2m_firmware_state_set(0, STATE_DOWNLOADED);

	return 0;
}

/* In case of error:
 * for VzW, we retry on network and protocol errors
 * for AT&T, we only retry protocol errors
 *
 * We retry on network errors with VzW because they can happen and
 * we don't trust VzW to retry, they would just fail the test :)
 * In case of AT&T they expect us to report an error instead.
 *
 * We retry on protocol errors (-EBADMSG) because
 * I have seen Motive servers send a partial content after 3 attempts.
 * This behavior could happen with other servers as well.

 * -EBADMSG indicates an unexpected HTTP response.
 * This could be due to the URI being wrong, or the server
 * not sending "Content-Range" or the file size in the response.
 */
static int on_error(const struct lwm2m_os_download_evt *event)
{
	LWM2M_WRN("Download interrupted, reason %d", event->error);

	lwm2m_os_download_disconnect();
	/* Close the DFU socket, we need memory for handshaking TLS again */
	dfusock_close();

	if (download_retry_and_update()) {
		/* Retry the download.
		 * Do not restart the download via this handler.
		 * We have closed the DFU socket and must re-set
		 * the offset before we send data to the modem again.
		 * Let the download_task handle that.
		 */
		lwm2m_os_timer_start(download_dwork,
			(event->error == -EBADMSG) ?
			NO_WAIT :	/* proto err, retry now */
			SECONDS(20)	/* net err, retry later */
		);
		return -1;
	}

	/* We have reached the maximum number of retries, give up */
	lwm2m_firmware_state_set(0, STATE_IDLE);
	/* Do not attemp to download again */
	lwm2m_firmware_image_state_set(FIRMWARE_NONE);

	if (event->error == -EBADMSG) {
		/* Protocol error */
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_INVALID_URI);
		carrier_error_evt_send(
			LWM2M_CARRIER_ERROR_FOTA_PROTO, event->error);
	} else {
		/* Network error */
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_CONN_LOST);
		carrier_error_evt_send(
			LWM2M_CARRIER_ERROR_FOTA_CONN_LOST, event->error);
	}

	/* Stop the download */
	return -1;
}

static int callback(const struct lwm2m_os_download_evt *event)
{
	switch (event->id) {
	case LWM2M_OS_DOWNLOAD_EVT_FRAGMENT:
		return on_fragment(event);
	case LWM2M_OS_DOWNLOAD_EVT_DONE:
		return on_done(event);
	case LWM2M_OS_DOWNLOAD_EVT_ERROR:
		return on_error(event);
	default:
		return 0;
	}
}

static void lte_link_down(void)
{
	LWM2M_INF("Link down to erase firmware image");
	lwm2m_request_link_down();
}

static void lte_link_up(void)
{
	LWM2M_INF("Restablishing LTE connection");
	lwm2m_request_link_up();
}

static void delete_task(void *w)
{
	int err;
	uint32_t off;
	uint32_t net_reg;
	static bool turn_link_on;

	err = dfusock_init();
	if (err) {
		return;
	}

	net_reg = lwm2m_net_reg_stat_get();
	if (net_reg != NET_REG_OFFLINE) {
		turn_link_on = true;
		lte_link_down();
		lwm2m_os_timer_start(delete_dwork, NETWORK_POLL_INTERVAL);
		return;
	}

	err = dfusock_offset_get(&off);
	if (err) {
		/* Operation is pending, wait until it has completed */
		LWM2M_INF("Waiting for firmware to be deleted..");
		lwm2m_os_timer_start(delete_dwork, OFFSET_POLL_INTERVAL);
		return;
	}

	LWM2M_INF("Offset retrieved: %lu", off);
	if (off == DIRTY_IMAGE) {
		LWM2M_INF("Deleting existing firmware in flash");
		err = dfusock_firmware_delete();
		if (err) {
			return;
		}
		/* Wait until operation has completed */
		lwm2m_os_timer_start(delete_dwork, OFFSET_POLL_INTERVAL);
		return;
	}

	if (turn_link_on && (net_reg == NET_REG_OFFLINE)) {
		lte_link_up();
	}

	dfusock_close();
}

static void download_task(void *w)
{
	int err;
	uint32_t off;
	enum lwm2m_firmware_image_state state = FIRMWARE_NONE;

	err = dfusock_init();
	if (err) {
		/* Error is already logged, reschedule in 1 minute */
		lwm2m_os_timer_start(download_dwork, MINUTES(1));
		return;
	}

	err = dfusock_offset_get(&off);
	if (err) {
		return;
	}

	LWM2M_INF("Offset retrieved: %lu", off);

	/* We rely on the information in flash to interpret whether a non-zero,
	 * non-dirty firmware offset is a complete firmware image or not.
	 */
	if (off != 0 && off != DIRTY_IMAGE) {
		err = lwm2m_firmware_image_state_get(&state);
		if (!err && state == FIRMWARE_READY) {
			LWM2M_INF("Image already present");
			lwm2m_firmware_state_set(0, STATE_DOWNLOADED);
			return;
		}
	}

	/* We are downloading a new firmware image */
	lwm2m_firmware_image_state_set(FIRMWARE_DOWNLOADING_PULL);

	LWM2M_INF("%s download", off ? lwm2m_os_log_strdup("Resuming") :
				       lwm2m_os_log_strdup("Starting"));

	/* Offset must be explicitly set when non-zero */
	dfusock_offset_set(off);

	err = lwm2m_os_download_connect(package_url, &config);
	if (err) {
		LWM2M_ERR("Failed to connect %d", err);
		if (err == -ENETUNREACH) {
			/* In -this- case this means the PDN is down.
			 * This is propagated from bind() returning EINVAL,
			 * in the download_client_connect() call.
			 * FOTA is triggered either by the server, which
			 * prompts us to update to receive a new message,
			 * or by us in case we resume the download on boot.
			 * In both situations we can rely on the main logic
			 * to setup the PDN as necessary, thus here we can
			 * just wait for the PDN to be brought up.
			 */
			lwm2m_os_timer_start(download_dwork, NETWORK_POLL_INTERVAL);
			return;
		}

		lwm2m_firmware_state_set(0, STATE_IDLE);
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_INVALID_URI);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_CONN, 0);
		return;
	}

	err = lwm2m_os_download_start(package_url, off);
	if (err) {
		lwm2m_firmware_state_set(0, STATE_IDLE);
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_CONN_LOST);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_CONN_LOST, 0);
		return;
	}
}

static void reboot_task(void *w)
{
	int battery;

	battery = lwm2m_device_battery_status_get();
	if (battery == LWM2M_CARRIER_BATTERY_STATUS_LOW_BATTERY) {
		LWM2M_INF("Battery low - firmware update deferred by 5 minutes");
		lwm2m_os_timer_start(reboot_dwork, MINUTES(5));
		return;
	}

	LWM2M_INF("Firmware update scheduled at boot");
	lwm2m_firmware_state_set(0, STATE_UPDATING);

	/* Reset to continue FOTA update */
	lwm2m_request_reset();
}

int lwm2m_firmware_download_init(void)
{
	int err;
	uint32_t off;
	uint8_t saved_ver[UUID_LEN];
	uint8_t cur_ver[PRINTABLE_UUID_LEN];
	enum lwm2m_firmware_update_state update;
	enum lwm2m_firmware_image_state img = FIRMWARE_NONE;
	/* Silence warnings */
	(void)img_state_str;

	download_dwork = lwm2m_os_timer_get(download_task);
	if (!download_dwork) {
		return -1;
	}

	delete_dwork = lwm2m_os_timer_get(delete_task);
	if (!delete_dwork) {
		return -1;
	}

	reboot_dwork = lwm2m_os_timer_get(reboot_task);
	if (!reboot_dwork) {
		return -1;
	}

	err = lwm2m_os_download_init(callback);
	if (err) {
		return err;
	}

	err = dfusock_init();
	if (err) {
		return err;
	}

	err = dfusock_version_get(cur_ver, PRINTABLE_UUID_LEN);
	if (err) {
		return err;
	}

	/* dfusock_version_get() will NULL-terminate the version string */
	LWM2M_INF("Modem firmware version: %s", lwm2m_os_log_strdup(cur_ver));

	err = dfusock_flash_size_get(&flash_size);
	if (err) {
		return err;
	}

	LWM2M_INF("Flash size: %d", flash_size);

	/* Check if a firmware update has happened and compare modem versions */
	err = lwm2m_firmware_update_state_get(&update);
	if (!err && update == UPDATE_EXECUTED) {
		lwm2m_last_firmware_version_get(saved_ver, UUID_LEN);
		if (memcmp(cur_ver, saved_ver, UUID_LEN)) {
			LWM2M_INF("Firmware updated!");
			lwm2m_firmware_update_result_set(0, RESULT_SUCCESS);
		} else {
			LWM2M_WRN("Firmware NOT updated!");
			lwm2m_firmware_update_result_set(0, RESULT_ERROR_UPDATE_FAILED);
			carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_FAIL, 0);
		}

		err = lwm2m_firmware_update_state_set(UPDATE_NONE);
		if (err) {
			return err;
		}
	}

	err = dfusock_offset_get(&off);
	if (err) {
		return err;
	}

	LWM2M_INF("Firmware offset: %d", off);

	lwm2m_firmware_image_state_get(&img);
	if (img != FIRMWARE_NONE) {
		LWM2M_INF("Firmware image state: %s",
			lwm2m_os_log_strdup(img_state_str[img]));

		if (img == FIRMWARE_READY) {
			lwm2m_firmware_state_set(0, STATE_DOWNLOADED);
		}
	}

	/* Because the firmware can be pushed to the device (in-band FOTA),
	 * we must be ready to receive it into a blank area since erasing it
	 * on the fly takes too long. We can't resume downloading firmware
	 * that is being pushed, so delete it if found.
	 */
	if (off == DIRTY_IMAGE || (off != 0 && img == FIRMWARE_DOWNLOADING_PUSH)) {
		dfusock_firmware_delete();
		do {
			LWM2M_INF("Waiting for firmware to be deleted");
			lwm2m_os_sleep(OFFSET_POLL_INTERVAL);
			err = dfusock_offset_get(&off);
		} while (err && off != 0);
		lwm2m_firmware_image_state_set(FIRMWARE_NONE);
	}

	/* Close the DFU socket so the application can use it */
	dfusock_close();

	return 0;
}

int lwm2m_firmware_download_resume(void)
{
	int err;
	char url[URL_SIZE] = {0};
	enum lwm2m_firmware_image_state img = FIRMWARE_NONE;

	/* Check if there is a download to resume.
	 * We can only resume downloading images which we were pulling.
	 */
	err = lwm2m_firmware_image_state_get(&img);

	if (!err && img == FIRMWARE_DOWNLOADING_PULL) {
		err = lwm2m_firmware_uri_get(url, sizeof(url));
		if (!err) {
			/* Resume */
			LWM2M_INF("Resuming download after power loss");
			lwm2m_firmware_download_uri(url, sizeof(url));
		} else {
			/* Should not happen */
			LWM2M_WRN("No URI to resume firmware update!");
		}
	}

	return err;
}

int lwm2m_firmware_download_uri(char *uri, size_t len)
{
	char *p;

	if (len >= sizeof(package_url)) {
		return -ENOMEM;
	}

	memcpy(package_url, uri, len);
	package_url[len] = '\0';

	LWM2M_INF("Package URI: %s (%d)",
		  lwm2m_os_log_strdup(package_url), strlen(package_url));

	/* Find the start of the hostname */
	p = strstr(package_url, "//");
	if (!p) {
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_UNSUP_PROTO);
		return -EINVAL;
	}

	if (!strncmp(package_url, "https", strlen("https"))) {
		config.sec_tag = CONFIG_NRF_LWM2M_CARRIER_SEC_TAG;
	} else if (IS_ENABLED(CONFIG_COAP) &&
		   !strncmp(package_url, "coaps", strlen("coaps"))) {
		config.sec_tag = operator_is_vzw(true) ? VZW_DM_SEC_TAG :
							 ATT_DM_SEC_TAG;
	} else {
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_UNSUP_PROTO);
		return -EINVAL;
	}

	/* Save the URL to resume the download on boot after a power loss */
	lwm2m_firmware_uri_set(package_url, strlen(package_url));

	/* Setup PDN, unless debugging */
	if (!lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
		len = lwm2m_carrier_apn_get(apn, sizeof(apn));
		if (len > 0) {
			config.apn = apn;
			LWM2M_INF("Setting up APN for HTTP download: %s",
				  lwm2m_os_log_strdup(config.apn));
		} else {
			config.apn = NULL;
		}
	}

	/* Set state now, since the actual download might be delayed in case
	 * there is a firmware image in flash that needs to be deleted.
	 */
	lwm2m_firmware_state_set(0, STATE_DOWNLOADING);
	lwm2m_firmware_update_result_set(0, RESULT_DEFAULT);

	/* Global flag to check file size */
	check_file_size = true;

	carrier_evt_send(LWM2M_CARRIER_EVENT_FOTA_START, package_url);
	lwm2m_os_timer_start(download_dwork, NO_WAIT);

	return 0;
}

void lwm2m_firmware_download_reboot_schedule(int timeout)
{
	lwm2m_os_timer_start(reboot_dwork, timeout);
}

int lwm2m_firmware_download_apply(void)
{
	int err;
	uint8_t ver[UUID_LEN];
	enum lwm2m_firmware_image_state state;
	nrf_dfu_err_t dfu_err;

	err = lwm2m_firmware_image_state_get(&state);

	if (!err && state != FIRMWARE_READY) {
		/* Request should not have come at this time. */
		LWM2M_WRN("Ignoring update request, not ready yet.");
		return -ENFILE;
	}

	err = dfusock_init();
	if (err) {
		return err;
	}

	err = dfusock_version_get(ver, UUID_LEN);
	if (err) {
		return err;
	}

	err = lwm2m_last_firmware_version_set(ver, UUID_LEN);
	if (err) {
		return err;
	}

	/* We won't need to re-download / re-apply this image */
	err = lwm2m_firmware_image_state_set(FIRMWARE_NONE);
	if (err) {
		return err;
	}

	err = dfusock_firmware_update();
	if (err) {
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_CRC);

		/* Notify application */
		dfusock_error_get(&dfu_err);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_PKG, dfu_err);

		return err;
	}

	dfusock_close();

	err = lwm2m_firmware_update_state_set(UPDATE_SCHEDULED);
	if (err) {
		return err;
	}

	return 0;
}
