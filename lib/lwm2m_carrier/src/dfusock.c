/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stddef.h>
#include <lwm2m.h>
#include <nrf_socket.h>
#include <nrf_errno.h>
#include <zephyr.h> /* For __ASSERT_NO_MSG */

static int dfusock = -1;

int dfusock_error_get(nrf_dfu_err_t *dfu_err)
{
	int err;
	nrf_socklen_t len;

	len = sizeof(*dfu_err);
	err = nrf_getsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_ERROR,
			     dfu_err, &len);
	if (err) {
		LWM2M_ERR("Unable to fetch modem error, err %d",
			  -lwm2m_os_errno());
	}

	return err;
}

int dfusock_fragment_send(const void *buf, size_t len)
{
	int sent;

	__ASSERT_NO_MSG(buf);

	LWM2M_INF("Sending fragment (%u) to modem..", len);

	sent = nrf_send(dfusock, buf, len, 0);
	if (sent < 0) {
		int err;

		err = -lwm2m_os_errno();
		LWM2M_ERR("Modem rejected fragment, err %d", err);
		return err;
	}

	return 0;
}

int dfusock_offset_get(uint32_t *off)
{
	int err;
	nrf_socklen_t len;

	__ASSERT_NO_MSG(off);

	len = sizeof(*off);
	err = nrf_getsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_OFFSET, off,
			     &len);
	if (err) {
		/* When polling the offset while the modem scratch area
		 * is being erased, we expect -NRF_ENOEXEC while the
		 * operation is ongoing.
		 * Return this value to the caller.
		 */
		return -lwm2m_os_errno();
	}

	return 0;
}

int dfusock_offset_set(uint32_t off)
{
	int err;
	nrf_socklen_t len;

	len = sizeof(off);
	err = nrf_setsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_OFFSET, &off,
			     len);
	if (err) {
		LWM2M_ERR("Failed to set offset, err %d", -lwm2m_os_errno());
	}

	return err;
}

int dfusock_flash_size_get(uint32_t *size)
{
	int err;
	nrf_socklen_t len;

	len = sizeof(*size);
	err = nrf_getsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_RESOURCES, size,
			     &len);
	if (err) {
		LWM2M_ERR("Unable to retrieve flash size, err %d",
			  -lwm2m_os_errno());
	}

	return err;
}

int dfusock_version_get(uint8_t *buf, size_t len)
{
	int err;
	nrf_socklen_t ver_len;

	__ASSERT_NO_MSG(len >= sizeof(nrf_dfu_fw_version_t));

	ver_len = sizeof(nrf_dfu_fw_version_t);
	err = nrf_getsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_FW_VERSION, buf,
			     &ver_len);
	if (err) {
		LWM2M_ERR("Failed to read firmware version, err %d",
			  -lwm2m_os_errno());
		return -1;
	}

	/* NULL terminate, if buffer is large enough */
	if (len > sizeof(nrf_dfu_fw_version_t)) {
		*((uint8_t *)buf + sizeof(nrf_dfu_fw_version_t)) = '\0';
	}

	return 0;
}

int dfusock_firmware_delete(void)
{
	int err;

	err = nrf_setsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_BACKUP_DELETE,
			     NULL, 0);
	if (err) {
		LWM2M_ERR("Failed to delete firmware, err %d",
			  -lwm2m_os_errno());
	}

	return err;
}

int dfusock_firmware_update(void)
{
	int err;

	err = nrf_setsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_APPLY, NULL, 0);
	if (err) {
		LWM2M_ERR("Failed to apply firmware update, err %d",
			  -lwm2m_os_errno());
	}

	return err;
}

int dfusock_firmware_revert(void)
{
	int err;

	err = nrf_setsockopt(dfusock, NRF_SOL_DFU, NRF_SO_DFU_REVERT, NULL, 0);
	if (err) {
		LWM2M_ERR("Failed to rollaback firmware, err %d",
			  -lwm2m_os_errno());
	}

	return err;
}

int dfusock_close(void)
{
	int err;

	if (dfusock == -1) {
		return 0;
	}

	err = nrf_close(dfusock);
	if (err) {
		LWM2M_ERR("Failed to close DFU socket, err %d",
			  -lwm2m_os_errno());
	}

	dfusock = -1;

	return err;
}

int dfusock_init(void)
{
	if (dfusock != -1) {
		LWM2M_TRC("DFU socket already open");
		return 0;
	}

	/* Ready DFU socket */
	dfusock = nrf_socket(NRF_AF_LOCAL, NRF_SOCK_STREAM, NRF_PROTO_DFU);
	if (dfusock < 0) {
		LWM2M_ERR("Failed to open DFU socket, err %d",
			  -lwm2m_os_errno());
		return -1;
	}

	return 0;
}