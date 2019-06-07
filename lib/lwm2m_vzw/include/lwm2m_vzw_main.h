/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_VZW_MAIN_H__

#include <net/socket.h>

typedef enum
{
    LWM2M_STATE_BOOTING,
    LWM2M_STATE_IDLE,
    LWM2M_STATE_IP_INTERFACE_UP,
    LWM2M_STATE_BS_CONNECT,
    LWM2M_STATE_BS_CONNECT_WAIT,
    LWM2M_STATE_BS_CONNECT_RETRY_WAIT,
    LWM2M_STATE_BS_CONNECTED,
    LWM2M_STATE_BOOTSTRAP_REQUESTED,
    LWM2M_STATE_BOOTSTRAP_WAIT,
    LWM2M_STATE_BOOTSTRAP_TIMEDOUT,
    LWM2M_STATE_BOOTSTRAPPING,
    LWM2M_STATE_BOOTSTRAP_HOLDOFF,
    LWM2M_STATE_SERVER_CONNECT,
    LWM2M_STATE_SERVER_CONNECT_WAIT,
    LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT,
    LWM2M_STATE_SERVER_CONNECTED,
    LWM2M_STATE_SERVER_REGISTER_WAIT,
    LWM2M_STATE_SERVER_DEREGISTER,
    LWM2M_STATE_SERVER_DEREGISTERING,
    LWM2M_STATE_DISCONNECT,
    LWM2M_STATE_MODEM_FIRMWARE_UPDATE,
    LWM2M_STATE_SHUTDOWN,
} lwm2m_state_t;

lwm2m_state_t lwm2m_state_get(void);
void lwm2m_state_set(lwm2m_state_t lwm2m_state);
char *lwm2m_imei_get(void);
char *lwm2m_msisdn_get(void);

void lwm2m_request_server_update(uint16_t instance_id, bool reconnect);
bool lwm2m_did_bootstrap(void);
uint16_t lwm2m_server_instance(void);
sa_family_t lwm2m_family_type_get(uint16_t instance_id);
int32_t lwm2m_state_update_delay(void);

void lwm2m_bootstrap_reset(void);
void lwm2m_factory_reset(void);
void lwm2m_system_shutdown(void);
void lwm2m_system_reset(void);

#endif // LWM2M_VZW_MAIN_H__
