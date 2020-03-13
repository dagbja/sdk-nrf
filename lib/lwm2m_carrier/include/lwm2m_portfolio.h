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

// LWM2M core resources.

void lwm2m_portfolio_init(void);

lwm2m_portfolio_t * lwm2m_portfolio_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_portfolio_get_object(void);

uint32_t lwm2m_portfolio_object_callback(lwm2m_object_t * p_object,
                                         uint16_t         instance_id,
                                         uint8_t          op_code,
                                         coap_message_t * p_request);

void lwm2m_portfolio_init_acl(void);

#endif /* LWM2M_PORTFOIO_H__ */