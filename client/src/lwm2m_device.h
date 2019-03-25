/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>

void lwm2m_device_init(void);

lwm2m_device_t * lwm2m_device_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_device_get_object(void);

char * lwm2m_device_get_sim_iccid(uint32_t * iccid_len);