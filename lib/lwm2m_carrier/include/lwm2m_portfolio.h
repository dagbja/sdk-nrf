/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_PORTFOLIO_H__
#define LWM2M_PORTFOLIO_H__

#include <stdint.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

#define LWM2M_PRIMARY_HOST_DEVICE_PORTFOLIO 0
#define LWM2M_PORTFOLIO_CARRIER_INSTANCE 2

void lwm2m_portfolio_init(void);

lwm2m_portfolio_t * lwm2m_portfolio_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_portfolio_get_object(void);

int lwm2m_portfolio_instance_create(uint16_t instance_id);

uint32_t lwm2m_portfolio_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request);

const void * lwm2m_portfolio_resource_reference_get(uint16_t instance_id, uint16_t resource_id, uint8_t *p_type);

#endif /* LWM2M_PORTFOIO_H__ */