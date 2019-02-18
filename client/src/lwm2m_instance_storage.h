/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>

#ifndef LWM2M_INSTANCE_STORAGE_H__
#define LWM2M_INSTANCE_STORAGE_H__

typedef struct __attribute__((__packed__))
{
    uint8_t bootstrapped;
} lwm2m_instance_storage_misc_data_t;

int32_t lwm2m_instance_storage_init(void);
int32_t lwm2m_instance_storage_deinit(void);

int32_t lwm2m_instance_storage_misc_data_load(lwm2m_instance_storage_misc_data_t * p_value);
int32_t lwm2m_instance_storage_misc_data_store(lwm2m_instance_storage_misc_data_t * p_value);
int32_t lwm2m_instance_storage_misc_data_delete(void);

int32_t lwm2m_instance_storage_security_load(uint16_t instance_id);
int32_t lwm2m_instance_storage_security_store(uint16_t instance_id);
int32_t lwm2m_instance_storage_security_delete(uint16_t instance_id);

int32_t lwm2m_instance_storage_server_load(uint16_t instance_id);
int32_t lwm2m_instance_storage_server_store(uint16_t instance_id);
int32_t lwm2m_instance_storage_server_delete(uint16_t instance_id);

int32_t lwm2m_last_used_msisdn_get(char * p_msisdn, uint8_t max_len);
int32_t lwm2m_last_used_msisdn_set(const char * p_msisdn, uint8_t len);

#endif // LWM2M_INSTANCE_STORAGE_H__