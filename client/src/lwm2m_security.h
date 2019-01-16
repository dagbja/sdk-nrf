/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>

char * lwm2m_security_server_uri_get(uint16_t instance_id);

void lwm2m_security_server_uri_set(uint16_t instance_id, char * value);

uint32_t lwm2m_security_is_bootstrap_server_get(uint16_t instance_id);

void lwm2m_security_is_bootstrap_server_set(uint16_t instance_id, bool value);

uint32_t lwm2m_security_bootstrapped_get(uint16_t instance_id);

void lwm2m_security_bootstrapped_set(uint16_t instance_id, uint32_t value);

uint32_t lwm2m_security_hold_off_timer_get(uint16_t instance_id);

void lwm2m_security_hold_off_timer_set(uint16_t instance_id, uint32_t value);

char * lwm2m_security_identity_get(uint16_t instance_id);

void lwm2m_security_identity_set(uint16_t instance_id, char * value);

char * lwm2m_security_psk_get(uint16_t instance_id);

void lwm2m_security_psk_set(uint16_t instance_id, char * value);

void lwm2m_security_init(void);

lwm2m_security_t * lwm2m_security_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_security_get_object(void);