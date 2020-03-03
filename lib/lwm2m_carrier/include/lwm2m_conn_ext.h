/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CONN_EXT_H__
#define LWM2M_CONN_EXT_H__

#include <stdint.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

// LWM2M core resources.

void lwm2m_conn_ext_init(void);

uint8_t lwm2m_conn_ext_apn_retries_get(uint16_t instance_id, uint16_t apn_instance);

int32_t lwm2m_conn_ext_apn_retry_period_get(uint16_t instance_id, uint16_t apn_instance);

int32_t lwm2m_conn_ext_apn_retry_back_off_period_get(uint16_t instance_id, uint16_t apn_instance);

lwm2m_connectivity_extension_t * lwm2m_conn_ext_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_conn_ext_get_object(void);

uint32_t lwm2m_conn_ext_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request);

void lwm2m_conn_ext_init_acl(void);

#endif /* LWM2M_CONN_EXT_H__ */
