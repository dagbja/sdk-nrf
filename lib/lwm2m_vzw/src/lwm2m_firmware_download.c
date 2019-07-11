/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <stdint.h>
#include <lwm2m.h>
#include <lwm2m_conn_mon.h>
#include <net/download_client.h>
#include <nrf_socket.h>

#include "lwm2m_objects.h"
#include "lwm2m_firmware.h"
#include "lwm2m_instance_storage.h"
#include "dfusock.h"

/* The offset is set to this value for dirty images, or backup images */
#define DIRTY_IMAGE 2621440
/* Modem UUID string length, without NULL terminatation */
#define UUID_LEN sizeof(nrf_dfu_fw_version_t)
/* Modem UUID string length, including NULL termination */
#define PRINTABLE_UUID_LEN (sizeof(nrf_dfu_fw_version_t) + 1)
/* Byte-length, without NULL termination */
#define BYTELEN(string) (sizeof(string) - 1)

static char apn[64];
static char file[256];
static char host[128];

static void *download_dwork;
static struct download_client http_downloader;
static struct download_client_cfg config = {
	.sec_tag = CONFIG_NRF_LWM2M_VZW_SEC_TAG,
};

int lwm2m_firmware_download_uri(char *package_uri, size_t len);

static int on_fragment(const struct download_client_evt *event)
{
	int err;

	err = dfusock_fragment_send(event->fragment.buf, event->fragment.len);
	if (!err) {
		/* Continue the download */
		return 0;
	}

	/* The modem refused the fragment, can't recover */
	download_client_disconnect(&http_downloader);

	lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_IDLE);
	lwm2m_firmware_update_result_set(0,
		LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC);

	/* Re-initialize the DFU socket to free up memory
	 * that could be necessary for the TLS handshake.
	 */
	dfusock_close();
	dfusock_init();

	/* Stop the download */
	return -1;
}

static int on_done(const struct download_client_evt *event)
{
	LWM2M_INF("Download completed");

	download_client_disconnect(&http_downloader);

	/* Save state and notify the server */
	lwm2m_firmware_image_ready_set(true);
	lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_DOWNLOADED);

	/* Close the DFU socket to free up memory for TLS,
	 * it will be re-opened in lwm2m_firmware_download_apply().
	 */
	LWM2M_INF("Closing DFU socket");
	dfusock_close();

	return 0;
}

static int on_error(const struct download_client_evt *event)
{
	int err;

	LWM2M_WRN("Download interrupted");
	err = download_client_disconnect(&http_downloader);
	if (err) {
		LWM2M_ERR("Failed to resume download");
		lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_IDLE);
		lwm2m_firmware_update_result_set(0,
			LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CONN_LOST);

		return -1;
	}

	/* Re-initialize the DFU socket to free up memory
	 * that could be necessary for the TLS handshake.
	 */
	dfusock_close();
	err = dfusock_init();
	if (err) {
		return -1;
	}

	lwm2m_os_timer_start(download_dwork, K_SECONDS(1));

	return 0;
}

static int callback(const struct download_client_evt *event)
{
	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT:
		return on_fragment(event);
	case DOWNLOAD_CLIENT_EVT_DONE:
		return on_done(event);
	case DOWNLOAD_CLIENT_EVT_ERROR:
		return on_error(event);
	default:
		return 0;
	}
}

static void download_task(void *w)
{
	int err;
	uint32_t off;

	/* Is a complete firmware image present? */
	bool ready = false;

	err = dfusock_offset_get(&off);
	if (err) {
		if (err == -ENOEXEC) {
			/* Operation is pending, wait until it has completed */
			LWM2M_INF("Waiting for firmware to be deleted..");
		} else {
			/* Should not receive any other error, log this */
			LWM2M_WRN("Waiting for firmware to be deleted (%d)",
				  err);
		}
		lwm2m_os_timer_start(download_dwork, K_SECONDS(2));
		return;
	}

	LWM2M_INF("Offset retrieved: %lu", off);

	/* At the moment we do not know if a non-zero offset is a
	 * complete firmware image or not. We should send a HEAD request
	 * to the HTTP server and see if the offset matches and if not,
	 * attempt to resume.
	 *
	 * A non-zero offset may also indicate that the download cannot
	 * be resumed, in which case it is the same as the flash size returned
	 * by the dedicated RPC call; in that situation we should delete the
	 * firmware image in flash.
	 */
	if (off == DIRTY_IMAGE) {
		LWM2M_INF("Deleting existing firmware in flash");
		err = dfusock_firmware_delete();
		if (err) {
			return;
		}
		/* Wait until operation has completed */
		lwm2m_os_timer_start(download_dwork, K_SECONDS(1));
		return;
	} else if (off != 0) {
		err = lwm2m_firmware_image_ready_get(&ready);
		if (!err && ready) {
			LWM2M_INF("Image already present");
			lwm2m_firmware_state_set(
				0, LWM2M_FIRMWARE_STATE_DOWNLOADED);
			return;
		}
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
	err = download_client_connect(&http_downloader, host, &config);
	if (err) {
		lwm2m_firmware_update_result_set(
			0, LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI);
		return;
	}

	err = download_client_start(&http_downloader, file, off);
	if (err) {
		lwm2m_firmware_update_result_set(
			0, LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI);
		return;
	}

	return;
}

int lwm2m_firmware_download_init(void)
{
	int err;
	uint32_t off;
	enum lwm2m_firmware_update_state state;

	uint8_t saved_ver[UUID_LEN];
	uint8_t cur_ver[PRINTABLE_UUID_LEN];

	download_dwork = lwm2m_os_timer_get(download_task);
	if (!download_dwork) {
		return -1;
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

	err = lwm2m_firmware_update_state_get(&state);
	if (!err && state == UPDATE_EXECUTED) {
		/* Check update result by comparing modem firmware versions */
		lwm2m_last_firmware_version_get(saved_ver, UUID_LEN);

		if (memcmp(cur_ver, saved_ver, UUID_LEN)) {
			LWM2M_INF("Firmware updated!");
			lwm2m_firmware_update_result_set(
				0, LWM2M_FIRMWARE_UPDATE_RESULT_SUCCESS);
		} else {
			LWM2M_INF("Firmware NOT updated!");
			lwm2m_firmware_update_result_set(
				0, LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_FIRMWARE_UPDATE_FAILED);
		}

		/* Clear flag and save new modem firmware version */
		err = lwm2m_firmware_update_state_set(UPDATE_NONE);
		if (err) {
			return err;
		}
	}

	err = download_client_init(&http_downloader, callback);
	if (err) {
		return err;
	}

	LWM2M_INF("Firmware download ready");

	/* Check if there is a download to resume */
	err = dfusock_offset_get(&off);
	if (err) {
		return err;
	}

	/* Image offset must be not dirty and non-zero */
	if (off != DIRTY_IMAGE && off != 0) {
		bool ready = false;
		char uri[512] = {0};
		/* Check if image is complete, if not resume */
		err = lwm2m_firmware_image_ready_get(&ready);
		if (!err & !ready) {
			err = lwm2m_firmware_uri_get(uri, sizeof(uri));
			if (!err) {
				/* Resume */
				LWM2M_INF("Resuming after power loss");
				lwm2m_firmware_download_uri(uri, sizeof(uri));
			} else {
				LWM2M_WRN("No package URI to resume from");
			}
		}
	}

	return 0;
}

int lwm2m_firmware_download_uri(char *package_uri, size_t len)
{
	char *p;
	size_t partial_len;

	len = MIN(len, sizeof(file) - 1);
	package_uri[len] = '\0';

	LWM2M_INF("Package URI: %s", lwm2m_os_log_strdup(package_uri));

	/* Find the end of the HTTP host */
	p = strstr(package_uri, "https://");
	if (!p) {
		lwm2m_firmware_update_result_set(0,
			LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PROTOCOL);

		return -EINVAL;
	}

	size_t from = BYTELEN("https://");

	p = strchr(package_uri + BYTELEN("https://"), '/');

	/* Length of the HTTP host string */
	partial_len = (p - package_uri) - from;

	/* Copy HTTP host */
	memcpy(host, package_uri + from, partial_len);
	host[partial_len + 1] = '\0';

	LWM2M_INF("Host: %s (%d)", lwm2m_os_log_strdup(host), partial_len);

	/* Length of the HTTP resource string */
	partial_len = len - (p - package_uri);

	/* Copy HTTP resource */
	memcpy(file, p + 1, partial_len);
	file[partial_len + 1] = '\0';

	LWM2M_INF("Resource: %s (%d)", lwm2m_os_log_strdup(file), partial_len);

	/* Setup PDN.
	 * Do not set this in the download task, or it will crash badly.
	 */
	p =  lwm2m_conn_mon_class_apn_get(2, (uint8_t*)&len);
	if (p) {
		/* NULL-terminate */
		memcpy(apn, p, len);
		apn[len] = '\0';
		config.apn = apn;
		LWM2M_INF("Setting up apn for HTTP download: %s",
			  lwm2m_os_log_strdup(config.apn));
	} else {
		LWM2M_INF("No APN set.");
		config.apn = NULL;
	}

	/* Save package URI to resume automatically
	 * on boot after a power loss has occurred.
	 */
	lwm2m_firmware_uri_set(package_uri, strlen(package_uri));

	/* Set state now, since the actual download might be delayed in case
	 * there is a firmware image in flash that needs to be deleted.
	 */
	lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_DOWNLOADING);

	lwm2m_os_timer_start(download_dwork, K_MSEC(1));

	return 0;
}

int lwm2m_firmware_download_apply(void)
{
	int err;
	bool ready = true;
	uint8_t ver[UUID_LEN];

	err = lwm2m_firmware_image_ready_get(&ready);

	if (!err && !ready) {
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
	err = lwm2m_firmware_image_ready_set(false);
	if (err) {
		return err;
	}

	err = dfusock_firmware_update();
	if (err) {
		/* It's not necessary to clear our own 'image ready' flag
		 * since if the update fails, the offset will be flagged as
		 * dirty by the modem itself and that overrides our own flag.
		 */
		lwm2m_firmware_update_result_set(
			0, LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC);
		return err;
	}

	/* Ignore any errors, it is critical to set UPDATE_SCHEDULED */
	dfusock_close();

	err = lwm2m_firmware_update_state_set(UPDATE_SCHEDULED);
	if (err) {
		return err;
	}

	return 0;
}