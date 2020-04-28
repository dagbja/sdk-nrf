/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_APN_CONN_PROF_H__
#define LWM2M_APN_CONN_PROF_H__

#include <stdint.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

// LWM2M core resources.

void lwm2m_apn_conn_prof_init(void);

bool lwm2m_apn_conn_prof_activate(uint16_t instance_id, uint8_t reject_cause);

bool lwm2m_apn_conn_prof_deactivate(uint16_t instance_id);

char * lwm2m_apn_conn_prof_apn_get(uint16_t instance_id, uint8_t * p_len);

bool lwm2m_apn_conn_prof_enabled_set(uint16_t instance_id, bool enable_status);

bool lwm2m_apn_conn_prof_is_enabled(uint16_t instance_id);

uint16_t lwm2m_apn_conn_prof_default_instance(void);

lwm2m_apn_conn_prof_t * lwm2m_apn_conn_prof_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_apn_conn_prof_get_object(void);

uint32_t lwm2m_apn_conn_prof_object_callback(lwm2m_object_t * p_object,
                                             uint16_t         instance_id,
                                             uint8_t          op_code,
                                             coap_message_t * p_request);

void lwm2m_apn_conn_prof_apn_status_update(void);

uint32_t lwm2m_apn_conn_prof_custom_apn_set(const char * p_apn);

#endif /* LWM2M_APN_CONN_PROF_H__ */