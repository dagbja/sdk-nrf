/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_tlv.h>

// TODO: Move to new carrier specific extended object/header.
typedef struct {
    int32_t is_bootstrapped;
} vzw_bootstrap_security_settings_t;

// Verizon specific resources.
char * lwm2m_security_server_uri_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_security_server_uri_set(uint16_t instance_id, const char * p_value, uint8_t len);

bool lwm2m_security_is_bootstrap_server_get(uint16_t instance_id);

void lwm2m_security_is_bootstrap_server_set(uint16_t instance_id, bool value);

// LWM2M core resources.
int32_t lwm2m_security_client_hold_off_time_get(uint16_t instance_id);

void lwm2m_security_client_hold_off_time_set(uint16_t instance_id, int32_t value);

bool lwm2m_security_bootstrapped_get(void);

void lwm2m_security_bootstrapped_set(bool value);

char * lwm2m_security_identity_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_security_identity_set(uint16_t instance_id, char * p_value, uint8_t len);

char * lwm2m_security_psk_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_security_psk_set(uint16_t instance_id, char * p_value, uint8_t len);

char * lwm2m_security_sms_number_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_security_sms_number_set(uint16_t instance_id, char * p_value, uint8_t len);

uint16_t lwm2m_security_short_server_id_get(uint16_t instance_id);

void lwm2m_security_short_server_id_set(uint16_t instance_id, uint16_t value);

void lwm2m_security_init(void);

void lwm2m_security_reset(uint16_t instance_id);

lwm2m_security_t * lwm2m_security_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_security_get_object(void);

uint32_t tlv_security_carrier_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv);
uint32_t tlv_security_carrier_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len);