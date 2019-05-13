/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>

#define LWM2M_CONN_MON_30000_CLASS_APN_2  0
#define LWM2M_CONN_MON_30000_CLASS_APN_3  1
#define LWM2M_CONN_MON_30000_CLASS_APN_6  2
#define LWM2M_CONN_MON_30000_CLASS_APN_7  3

typedef struct
{
    lwm2m_string_t class_apn[4];
} vzw_conn_mon_class_apn_t;

// Verizon specific resources.

char * lwm2m_conn_mon_class2_apn_get(uint16_t instance_id, uint8_t * p_len);

char * lwm2m_conn_mon_class3_apn_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_conn_mon_class3_apn_set(uint16_t instance_id, char * p_value, uint8_t len);

char * lwm2m_conn_mon_class6_apn_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_conn_mon_class6_apn_set(uint16_t instance_id, char * p_value, uint8_t len);

char * lwm2m_conn_mon_class7_apn_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_conn_mon_class7_apn_set(uint16_t instance_id, char * p_value, uint8_t len);


// LWM2M core resources.

char * lwm2m_conn_mon_apn_get(uint16_t instance_id, uint8_t * p_len);

void lwm2m_conn_mon_init(void);

lwm2m_connectivity_monitoring_t * lwm2m_conn_mon_get_instance(uint16_t instance_id);

lwm2m_object_t * lwm2m_conn_mon_get_object(void);

uint32_t tlv_conn_mon_verizon_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv);

uint32_t lwm2m_conn_mon_object_callback(lwm2m_object_t * p_object,
                                        uint16_t         instance_id,
                                        uint8_t          op_code,
                                        coap_message_t * p_request);

void lwm2m_conn_mon_observer_process(void);

uint32_t tlv_conn_mon_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv);