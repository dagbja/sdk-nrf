/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stddef.h>
#include <lwm2m_objects.h>
#include <app_debug.h>

#ifndef LWM2M_INSTANCE_STORAGE_H__
#define LWM2M_INSTANCE_STORAGE_H__

typedef struct __attribute__((__packed__))
{
    uint8_t bootstrapped;
} lwm2m_storage_misc_data_t;

struct lwm2m_storage_version {
    uint8_t version;
} __attribute__((__packed__));

typedef struct lwm2m_storage_version lwm2m_storage_version_t;

int32_t lwm2m_instance_storage_init(void);
int32_t lwm2m_instance_storage_deinit(void);

int lwm2m_storage_security_load(void);
int lwm2m_storage_security_store(void);
int lwm2m_storage_security_delete(void);
int lwm2m_storage_server_load(void);
int lwm2m_storage_server_store(void);
int lwm2m_storage_server_delete(void);
int lwm2m_storage_access_control_load(void);
int lwm2m_storage_access_control_store(void);
int lwm2m_storage_access_control_delete(void);
int lwm2m_storage_location_load(void);
int lwm2m_storage_location_store(void);
int lwm2m_storage_location_delete(void);

int lwm2m_storage_apn_conn_prof_load(void);
int lwm2m_storage_apn_conn_prof_store(void);
int lwm2m_storage_apn_conn_prof_delete(void);

int lwm2m_storage_portfolio_load(void);
int lwm2m_storage_portfolio_store(void);
int lwm2m_storage_portfolio_delete(void);

int lwm2m_storage_conn_ext_load(void);
int lwm2m_storage_conn_ext_store(void);
int lwm2m_storage_conn_ext_delete(void);

int32_t lwm2m_storage_misc_data_load(lwm2m_storage_misc_data_t * p_value);
int32_t lwm2m_storage_misc_data_store(lwm2m_storage_misc_data_t * p_value);

int32_t lwm2m_last_used_msisdn_get(char * p_msisdn, uint8_t max_len);
int32_t lwm2m_last_used_msisdn_set(const char * p_msisdn, uint8_t len);

int32_t lwm2m_last_used_operator_id_get(uint32_t * p_operator_id);
int32_t lwm2m_last_used_operator_id_set(uint32_t operator_id);

int32_t lwm2m_debug_settings_load(debug_settings_t * debug_settings);
int32_t lwm2m_debug_settings_store(const debug_settings_t * debug_settings);

int lwm2m_last_firmware_version_get(uint8_t *ver, size_t len);
int lwm2m_last_firmware_version_set(uint8_t *ver, size_t len);

enum lwm2m_firmware_image_state {
	/* No valid firmware image, or invalid firmware image */
	FIRMWARE_NONE,
	/* Firmware is downloading (PULL) */
	FIRMWARE_DOWNLOADING_PULL,
	/* Firmware is downloading (PUSH) */
	FIRMWARE_DOWNLOADING_PUSH,
	/* Firmware has been downloaded */
	FIRMWARE_READY,
	/* Force enum size to int */
	_IMAGE_STATE_ENUM_SIZE = INT32_MAX,
};

int lwm2m_firmware_image_state_get(enum lwm2m_firmware_image_state *);
int lwm2m_firmware_image_state_set(enum lwm2m_firmware_image_state);

enum lwm2m_firmware_update_state {
	/* No update operation scheduled */
	UPDATE_NONE,
	/* Update scheduled for next reboot */
	UPDATE_SCHEDULED,
	/* Update executed during last reboot (either successfully or not) */
	UPDATE_EXECUTED,
	/* Force enum size to int */
	_UPDATE_STATE_ENUM_SIZE = INT32_MAX,
};

int lwm2m_firmware_update_state_get(enum lwm2m_firmware_update_state *);
int lwm2m_firmware_update_state_set(enum lwm2m_firmware_update_state);

/**
 * @brief Retrieve package URI from flash.
 *
 * @param uri Buffer to load the URI into
 * @param[in,out] len Size of the buffer on input, size of the URI on output
 * @return 0 on success, an errno otherwise.
 */
int lwm2m_firmware_uri_get(char *uri, size_t *len);

int lwm2m_firmware_uri_set(char *uri, size_t len);

int lwm2m_stored_class3_apn_read(char *class3_apn, size_t len);
int lwm2m_stored_class3_apn_write(char *class3_apn, size_t len);
int lwm2m_stored_class3_apn_delete(void);

int lwm2m_observer_store(uint32_t sid, void * data, size_t size);
int lwm2m_observer_load(uint32_t sid, void * data, size_t size);
int lwm2m_observer_delete(uint32_t sid);

int lwm2m_notif_attr_store(uint32_t sid, void * data, size_t size);
int lwm2m_notif_attr_load(uint32_t sid, void * data, size_t size);
int lwm2m_notif_attr_delete(uint32_t sid);

#endif // LWM2M_INSTANCE_STORAGE_H__
