/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_objects.h>

void lwm2m_device_update_carrier_specific_settings(void);
void lwm2m_device_init(void);

lwm2m_device_t * lwm2m_device_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_device_get_object(void);

char * lwm2m_device_get_sim_iccid(uint32_t * iccid_len);
int lwm2m_device_set_sim_iccid(char *p_iccid, uint32_t iccid_len);

int32_t lwm2m_device_battery_status_get(void);

const void * lwm2m_device_resource_reference_get(uint16_t resource_id, uint8_t *p_type);

int lwm2m_device_ext_dev_info_set(const int32_t* ext_dev_info, uint8_t ext_dev_info_count);

void lwm2m_device_ext_dev_info_clear(void);
