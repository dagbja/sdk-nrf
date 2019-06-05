/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_tlv.h>

typedef struct {
    uint32_t is_registered;
    uint32_t client_hold_off_timer;      /**< The number of seconds to wait before attempting bootstrap or registration. */
} vzw_server_settings_t;

// Verizon specific resources.
uint32_t lwm2m_server_registered_get(uint16_t instance_id);

void lwm2m_server_registered_set(uint16_t instance_id, uint32_t value);

uint32_t lwm2m_server_client_hold_off_timer_get(uint16_t instance_id);

void lwm2m_server_client_hold_off_timer_set(uint16_t instance_id, uint32_t value);

// LWM2M core resources.
time_t lwm2m_server_lifetime_get(uint16_t instance_id);

void lwm2m_server_lifetime_set(uint16_t instance_id, time_t value);

time_t lwm2m_server_min_period_get(uint16_t instance_id);

void lwm2m_server_min_period_set(uint16_t instance_id, time_t value);

time_t lwm2m_server_max_period_get(uint16_t instance_id);

void lwm2m_server_max_period_set(uint16_t instance_id, time_t value);

time_t lwm2m_server_disable_timeout_get(uint16_t instance_id);

void lwm2m_server_disable_timeout_set(uint16_t instance_id, time_t value);

bool lwm2m_server_notif_storing_get(uint16_t instance_id);

void lwm2m_server_notif_storing_set(uint16_t instance_id, bool value);

char * lwm2m_server_binding_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_server_binding_set(uint16_t instance_id, char * value, uint8_t len);

uint16_t lwm2m_server_short_server_id_get(uint16_t instance_id);

void lwm2m_server_short_server_id_set(uint16_t instance_id, uint16_t value);

void lwm2m_server_init(void);

lwm2m_server_t * lwm2m_server_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_server_get_object(void);

uint32_t lwm2m_server_object_callback(lwm2m_object_t * p_object,
                                      uint16_t         instance_id,
                                      uint8_t          op_code,
                                      coap_message_t * p_request);

uint32_t lwm2m_server_observer_process(void);

uint32_t tlv_server_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv);