/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CONN_STAT_H__
#define LWM2M_CONN_STAT_H__

#include <stdint.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

// LWM2M core resources.

void lwm2m_conn_stat_init(void);

lwm2m_connectivity_statistics_t * lwm2m_conn_stat_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_conn_stat_get_object(void);

uint32_t lwm2m_conn_stat_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request);

#endif /* LWM2M_CONN_STAT_H__ */
