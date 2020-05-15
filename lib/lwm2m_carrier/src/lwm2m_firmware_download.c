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
#include <lwm2m_objects.h>
#include <lwm2m_carrier.h>
#include <lwm2m_firmware.h>
#include <lwm2m_firmware_download.h>
#include <lwm2m_carrier_main.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_device.h>

#include <operator_check.h>

/* The offset is set to this value for dirty images, or backup images */
#define DIRTY_IMAGE 2621440
/* Modem UUID string length, without NULL terminatation */
#define UUID_LEN sizeof(nrf_dfu_fw_version_t)
/* Modem UUID string length, including NULL termination */
#define PRINTABLE_UUID_LEN (sizeof(nrf_dfu_fw_version_t) + 1)
/* Byte-length, without NULL termination */
#define BYTELEN(string) (sizeof(string) - 1)
/* Interval with which to poll the modem firmware offset to determine if the
 * erase operation has completed.
 */
#define OFFSET_POLL_INTERVAL K_SECONDS(2)
/* Interval at which to poll for network availability */
#define NETWORK_POLL_INTERVAL K_SECONDS(6)
/* Number of times to retry a download */
#define DOWNLOAD_RETRIES 16

static char apn[64];
static char file[256];
static char host[128];
static uint32_t flash_size;
static bool check_file_size;

/* Number of times to retry a download on socket or HTTP errors.
 * This excludes the HTTP server closing the connection, since
 * that is retried automatically by the download_client.
 */
static u8_t download_retries = DOWNLOAD_RETRIES;

static void *download_dwork;
static void *reboot_dwork;
static struct lwm2m_os_download_cfg config = {
	.sec_tag = CONFIG_NRF_LWM2M_CARRIER_SEC_TAG,
};

static const char *img_state_str[] = {
	[FIRMWARE_NONE] = "no image",
	[FIRMWARE_DOWNLOADING] = "downloading",
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

static int on_fragment(const struct lwm2m_os_download_evt *event)
{
	int err;
	nrf_dfu_err_t dfu_err = DFU_NO_ERROR;

	if (check_file_size && !file_size_check_valid()) {
		LWM2M_WRN("File size too large");
		/* Do not attemp to download again */
		lwm2m_firmware_image_state_set(FIRMWARE_NONE);
		lwm2m_firmware_state_set(0, STATE_IDLE);
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_STORAGE);
		/* Stop the download */
		return -1;
	}

	err = dfusock_fragment_send(event->fragment.buf, event->fragment.len);
	if (!err) {
		/* All good, continue the download */
		return 0;
	}

	/* The modem refused the fragment.
	 * We can try to recover from this error and reattempt the download.
	 * Disconnect the HTTP socket regardless of whether we will reattempt
	 * the download, since we'll open it again anyway if and when we do.
	 */
	lwm2m_os_download_disconnect();

	if (err == -NRF_ENOEXEC) {
		/* Let's fetch the RPC error reason. */
		dfusock_error_get(&dfu_err);

		/* It could happen, after a manual or specific firmware
		 * update, that the scratch area is not erased even
		 * though the offset reported by the modem is zero.
		 * After rejecting a fragment the modem will report a
		 * "dirty" offset, and the download task will erase
		 * the scratch area before re-starting the download.
		 */
		LWM2M_WRN("Reject reason %d", (int)dfu_err);
		if  (dfu_err == DFU_AREA_NOT_BLANK) {
			LWM2M_INF("Erasing flash area and retrying");
			lwm2m_os_timer_start(download_dwork, K_NO_WAIT);
			/* Stop the download, it will be restarted */
			return -1;
		}
	}

	/* We can't recover from here, simply give up. */

	lwm2m_firmware_state_set(0, STATE_IDLE);
	lwm2m_firmware_update_result_set(0, RESULT_ERROR_CRC);

	carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_PKG, dfu_err);

	/* Re-initialize the DFU socket to free up memory
	 * that could be necessary for the TLS handshake.
	 */
	dfusock_close();
	dfusock_init();

	/* Stop the download */
	return -1;
}

static int on_done(const struct lwm2m_os_download_evt *event)
{
	LWM2M_INF("Download completed");

	lwm2m_os_download_disconnect();

	/* Save state and notify the server */
	lwm2m_firmware_image_state_set(FIRMWARE_READY);
	lwm2m_firmware_state_set(0, STATE_DOWNLOADED);

	/* Close the DFU socket to free up memory for TLS,
	 * and re-open it in case a new download is started without
	 * this delta ever being applied. That shouldn't happen but we guard
	 * ourselves against incorrect server behavior, which would otherwise
	 * start the download with the DFU socket closed.
	 */
	LWM2M_INF("Closing DFU socket");
	dfusock_close();
	dfusock_init();

	return 0;
}

/* In case of error:
 * for VzW, we retry on network and protocol errors
 * for AT&T, we only retry protocol errors
 *
 * We retry on network errors with VzW because they can happen and
 * we don't trust them to resend the firmware, they would just fail the test :)
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
	int err;

	LWM2M_WRN("Download interrupted, reason %d", event->error);
	err = lwm2m_os_download_disconnect();
	if (err) {
		/* error is already logged, keep on going */
	}

	/* Re-initialize the DFU socket to free up memory
	 * that could be necessary for the TLS handshake.
	 */
	(void) dfusock_close();
	(void) dfusock_init();

	if (operator_is_vzw(true) || event->error == -EBADMSG) {
		if (download_retry_and_update()) {
			/* Retry the download.
			 * Do not restart the download via this handler.
			 * We have closed the DFU socket and must re-set
			 * the offset before we begin data to the modem again.
			 * Let the download_task handle that.
			 */
			lwm2m_os_timer_start(download_dwork,
				(event->error == -EBADMSG) ?
				K_NO_WAIT :	/* proto err, retry now */
				K_SECONDS(20)	/* net err, retry later */
			);
			return -1;
		}
	}

	/* We have reached the maximum number of retries, give up */
	lwm2m_firmware_state_set(0, STATE_IDLE);

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

	/* Give up */
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

static int erase_check_timeout(void)
{
	static uint32_t erase_duration_ms;

	erase_duration_ms += OFFSET_POLL_INTERVAL;
	if (erase_duration_ms <
	    K_SECONDS(CONFIG_NRF_LWM2M_CARRIER_ERASE_TIMEOUT_S)) {
		return 0;
	}

	LWM2M_WRN("Erase operation timed out");
	erase_duration_ms = 0;

	return 1;
}

static void link_down(void)
{
	LWM2M_INF("Link down to erase firmware image");
	lwm2m_request_link_down();
}

static void link_up(void)
{
	LWM2M_INF("Restablishing LTE connection");
	lwm2m_request_link_up();
}

static void download_task(void *w)
{
	int err;
	uint32_t off;
	static bool turn_link_on;
	enum lwm2m_firmware_image_state state = FIRMWARE_NONE;

	/* Fetch the offset to determine what to do next.
	 * If the offset is zero we just follow through, otherwise
	 * we either begin erasing the firmware if the image is dirty
	 * or resume the download if it isn't. If we erase the firmware,
	 * then we reschedule the task until the operation has completed
	 * and the offset has become zero.
	 */

	err = dfusock_offset_get(&off);
	if (err) {
		if (err == -NRF_ENOEXEC) {
			/* Operation is pending, wait until it has completed */
			LWM2M_INF("Waiting for firmware to be deleted..");
		} else {
			/* Should not receive any other error, log this */
			LWM2M_WRN("Waiting for firmware to be deleted (%d)",
				  err);
		}
		if (erase_check_timeout()) {
			link_down();
			turn_link_on = true;
		}
		lwm2m_os_timer_start(download_dwork, OFFSET_POLL_INTERVAL);
		return;
	}

	LWM2M_INF("Offset retrieved: %lu", off);

	if (turn_link_on) {
		link_up();
		turn_link_on = false;
		lwm2m_os_timer_start(download_dwork, NETWORK_POLL_INTERVAL);
		return;
	}

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
	lwm2m_firmware_image_state_set(FIRMWARE_DOWNLOADING);

	if (off == DIRTY_IMAGE) {
		LWM2M_INF("Deleting existing firmware in flash");
		err = dfusock_firmware_delete();
		if (err) {
			return;
		}
		/* Wait until operation has completed */
		lwm2m_os_timer_start(download_dwork, OFFSET_POLL_INTERVAL);
		return;
	}

	/* No image, or a resumable image */
	LWM2M_INF("%s download", off ? lwm2m_os_log_strdup("Resuming") :
				       lwm2m_os_log_strdup("Starting"));

	/* Offset must be explicitly set when non-zero */
	dfusock_offset_set(off);

	/* Connect as late as possible.
	 * Deleting a firmware image can take a long time, so we connect late
	 * to minimize the idle time on the socket and prevent the peer from
	 * closing the connection before we get a chance to begin downloading.
	 */
	err = lwm2m_os_download_connect(host, &config);
	if (err) {
		LWM2M_ERR("Failed to connect %d", err);
		if (lwm2m_os_errno() == NRF_ENETDOWN) {
			/* PDN is down, pass bootstrap instance because
			 * the bootstrap server uses VZWADMIN PDN.
			 */
			int retry_delay = 0;
			(void) lwm2m_carrier_pdn_activate(0, &retry_delay);
			lwm2m_os_timer_start(download_dwork, retry_delay);
			return;
		}

		lwm2m_firmware_update_result_set(0, RESULT_ERROR_INVALID_URI);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_CONN, 0);
		return;
	}

	err = lwm2m_os_download_start(file, off);
	if (err) {
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
		lwm2m_os_timer_start(reboot_dwork, K_MINUTES(5));
		return;
	}

	LWM2M_INF("Firmware update scheduled at boot");
	lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_UPDATING);

	/* Reset to continue FOTA update */
	lwm2m_request_reset();
}

int lwm2m_firmware_download_init(void)
{
	int err;
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

	/* Detect if a firmware update has just happened */
	err = lwm2m_firmware_update_state_get(&update);
	if (!err && update == UPDATE_EXECUTED) {
		/* Check update result by comparing modem firmware versions */
		lwm2m_last_firmware_version_get(saved_ver, UUID_LEN);

		if (memcmp(cur_ver, saved_ver, UUID_LEN)) {
			LWM2M_INF("Firmware updated!");
			lwm2m_firmware_update_result_set(0, RESULT_SUCCESS);
		} else {
			LWM2M_INF("Firmware NOT updated!");
			lwm2m_firmware_update_result_set(
				0, RESULT_ERROR_UPDATE_FAILED);
			carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_FAIL, 0);
		}

		/* Clear flag and save new modem firmware version */
		err = lwm2m_firmware_update_state_set(UPDATE_NONE);
		if (err) {
			return err;
		}
	}

	/* Check if image is complete, if not resume.
	 * We have to rely on the information in flash to determine
	 * whether or not to resume, because the offset alone does
	 * provide enough information to resume in these two cases:
	 *
	 * - Zero : if we began erasing, and lost power while erasing
	 * - Non-Dirty, Non-Zero: if the image in flash is complete or not
	 *
	 * The download task handles the erase of any firmware image in flash,
	 * if that is necessary.
	 */
	err = lwm2m_firmware_image_state_get(&img);

	LWM2M_INF("Firmware download ready (%s)",
		  lwm2m_os_log_strdup(img_state_str[img]));

	if (!err && img == FIRMWARE_DOWNLOADING) {
		char uri[512] = { 0 };
		err = lwm2m_firmware_uri_get(uri, sizeof(uri));
		if (!err) {
			/* Resume */
			LWM2M_INF("Resuming download after power loss");
			lwm2m_firmware_download_uri(uri, sizeof(uri));
		} else {
			/* Should not happen */
			LWM2M_WRN("No URI to resume firmware update!");
		}
	}

	return 0;
}

int lwm2m_firmware_download_uri(char *package_uri, size_t len)
{
	char *p;
	char port[8];
	size_t partial_len;

	len = MIN(len, sizeof(file) - 1);
	package_uri[len] = '\0';

	LWM2M_INF("Package URI: %s", lwm2m_os_log_strdup(package_uri));

	/* Find the start of the HTTP host */
	p = strstr(package_uri, "https://");
	if (!p) {
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_UNSUP_PROTO);
		return -EINVAL;
	}

	/* Save package URI to resume automatically
	 * on boot after a power loss has occurred.
	 */
	lwm2m_firmware_uri_set(package_uri, strlen(package_uri));

	/* Swallow protocol */
	package_uri += BYTELEN("https://");

	/* Find the end of the host:port string */
	p = strchr(package_uri, ':');
	if (!p) {
		LWM2M_INF("Default port");
		p = strchr(package_uri, '/');
	}

	/* Length of the HTTP host string */
	partial_len = (p - package_uri);

	/* Copy HTTP host */
	memcpy(host, package_uri, partial_len);
	host[partial_len] = '\0';

	LWM2M_INF("Host: %s (%d)", lwm2m_os_log_strdup(host), partial_len);

	/* Swallow host and delimiter, either ':' or '/' */
	package_uri += partial_len + 1;

	/* Parse non-default port, if specified */
	if (*p == ':') {
		/* Move at the end of the port */
		p = strchr(p, '/');
		partial_len = (p - package_uri);

		memcpy(port, package_uri, partial_len);
		port[partial_len] = '\0';

		LWM2M_INF("Port: %s", lwm2m_os_log_strdup(port));
		config.port = atoi(port);

		/* Swallow port and '/' delimiter */
		package_uri += partial_len + 1;
	}

	/* Length of the HTTP resource string */
	partial_len = strlen(package_uri);

	/* Copy HTTP resource */
	memcpy(file, package_uri, partial_len);
	file[partial_len] = '\0';

	LWM2M_INF("Resource: %s (%d)", lwm2m_os_log_strdup(file), partial_len);

	/* Setup PDN, unless debugging */
	if (!lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
		len = lwm2m_carrier_apn_get(apn, sizeof(apn));
		if (len > 0) {
			config.apn = apn;
			LWM2M_INF("Setting up apn for HTTP download: %s",
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

	carrier_evt_send(LWM2M_CARRIER_EVENT_FOTA_START, file);
	lwm2m_os_timer_start(download_dwork, K_NO_WAIT);

	return 0;
}

void lwm2m_firmware_download_reboot_schedule(void)
{
	lwm2m_os_timer_start(reboot_dwork, K_NO_WAIT);
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
		/* It's not necessary to clear our own 'image ready' flag
		 * since if the update fails, the offset will be flagged as
		 * dirty by the modem itself and that overrides our own flag.
		 */
		lwm2m_firmware_update_result_set(0, RESULT_ERROR_CRC);

		/* Notify application */
		dfusock_error_get(&dfu_err);
		carrier_error_evt_send(LWM2M_CARRIER_ERROR_FOTA_PKG, dfu_err);

		return err;
	}

	/* Ignore any errors, it is critical to set UPDATE_SCHEDULED */
	(void) dfusock_close();

	err = lwm2m_firmware_update_state_set(UPDATE_SCHEDULED);
	if (err) {
		return err;
	}

	return 0;
}
