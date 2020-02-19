/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_objects.h>

#ifndef LWM2M_FIRWMARE_H__
#define LWM2M_FIRMWARE_H__

void lwm2m_firmware_init_acl(void);
void lwm2m_firmware_init(void);

char * lwm2m_firmware_package_uri_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_firmware_package_uri_set(uint16_t instance_id, char * p_value, uint8_t len);

uint8_t lwm2m_firmware_state_get(uint16_t instance_id);

void lwm2m_firmware_state_set(uint16_t instance_id, uint8_t value);

uint8_t lwm2m_firmware_update_result_get(uint16_t instance_id);

void lwm2m_firmware_update_result_set(uint16_t instance_id, uint8_t value);

uint8_t * lwm2m_firmware_firmware_update_protocol_support_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_firmware_firmware_update_protocol_support_set(uint16_t instance_id, uint8_t * p_value, uint8_t len);

uint8_t lwm2m_firmware_firmware_delivery_method_get(uint16_t instance_id);

void lwm2m_firmware_firmware_delivery_method_set(uint16_t instance_id, uint8_t value);

lwm2m_firmware_t * lwm2m_firmware_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_firmware_get_object(void);

void lwm2m_firmware_observer_process(struct nrf_sockaddr * p_remote_server);

#endif // LWM2M_FIRWMARE_H__