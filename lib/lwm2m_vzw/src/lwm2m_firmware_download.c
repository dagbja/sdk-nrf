/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <stdint.h>
#include <lwm2m.h>
#include "lwm2m_firmware.h"
#include "lwm2m_instance_storage.h"
#include <lwm2m_conn_mon.h>
#include "lwm2m_objects.h"
#include "dfusock.h"
#include <download_client.h>
#include <nrf_socket.h>

/* The offset is set to this value for dirty images, or backup images */
#define DIRTY_IMAGE 2621440
/* Modem UUID string length, without NULL terminatation */
#define UUID_LEN sizeof(nrf_dfu_fw_version_t)
/* Modem UUID string length, including NULL termination */
#define PRINTABLE_UUID_LEN (sizeof(nrf_dfu_fw_version_t) + 1)
/* Byte-length, without NULL termination */
#define BYTELEN(string) (sizeof(string) - 1)

static char pdn[64];
static char file[256];
static char host[128];

static void *download_dwork;

static struct download_client_cfg config = {
	.sec_tag = CONFIG_NRF_LWM2M_VZW_SEC_TAG,
};

static struct download_client http_downloader;

static int callback(const struct download_client_evt *event)
{
	int err;

	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT:
		err = dfusock_fragment_send(event->fragment.buf,
					    event->fragment.len);
		if (err) {
			/* TODO: ensure download can be restarted? */
			download_client_disconnect(&http_downloader);
			lwm2m_firmware_update_result_set(
				0, LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC);
			lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_IDLE);
			/* Stop the download */
			return -1;
		}
		break;

	case DOWNLOAD_CLIENT_EVT_DONE:
		/* Ready to apply patch on execute */
		LWM2M_INF("Download completed");
		download_client_disconnect(&http_downloader);
		lwm2m_firmware_image_ready_set(true);
		lwm2m_firmware_state_set(0, LWM2M_FIRMWARE_STATE_DOWNLOADED);
		break;

	case DOWNLOAD_CLIENT_EVT_ERROR:
		LWM2M_WRN("Download interrupted");
		err = download_client_disconnect(&http_downloader);
		if (err) {
			LWM2M_WRN("Failed to close HTTP socket, err %d", err);
		}
		LWM2M_INF("Socket closed");
		if (err) {
			LWM2M_ERR("Failed to resume download");
			lwm2m_firmware_update_result_set(
				0,
				LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CONN_LOST);
			break;
		}
		lwm2m_os_timer_start(download_dwork, K_SECONDS(1));
		break;
	}

	return 0;
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

	/* Offset must be explicitly set */
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
		memcpy(pdn, p, len);
		pdn[len] = '\0';
		config.pdn = pdn;
		LWM2M_INF("Setting up PDN for HTTP download: %s",
			  lwm2m_os_log_strdup(config.pdn));
	} else {
		LWM2M_INF("No PDN set.");
		config.pdn = NULL;
	}

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

	err = lwm2m_firmware_update_state_set(UPDATE_SCHEDULED);
	if (err) {
		return err;
	}

	return 0;
}