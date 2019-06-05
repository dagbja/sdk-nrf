/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_VZW_MAIN_H__

#include <net/socket.h>

int lwm2m_vzw_init(void);
void lwm2m_vzw_run(void);

#define SECURITY_SERVER_URI_SIZE_MAX 64    /**< Max size of server URIs. */

typedef enum
{
    APP_STATE_IDLE,
    APP_STATE_IP_INTERFACE_UP,
    APP_STATE_BS_CONNECT,
    APP_STATE_BS_CONNECT_WAIT,
    APP_STATE_BS_CONNECT_RETRY_WAIT,
    APP_STATE_BS_CONNECTED,
    APP_STATE_BOOTSTRAP_REQUESTED,
    APP_STATE_BOOTSTRAP_WAIT,
    APP_STATE_BOOTSTRAP_TIMEDOUT,
    APP_STATE_BOOTSTRAPPING,
    APP_STATE_BOOTSTRAPPED,
    APP_STATE_SERVER_CONNECT,
    APP_STATE_SERVER_CONNECT_WAIT,
    APP_STATE_SERVER_CONNECT_RETRY_WAIT,
    APP_STATE_SERVER_CONNECTED,
    APP_STATE_SERVER_REGISTER_WAIT,
    APP_STATE_SERVER_REGISTERED,
    APP_STATE_SERVER_DEREGISTER,
    APP_STATE_SERVER_DEREGISTERING,
    APP_STATE_DISCONNECT,
    APP_STATE_SHUTDOWN
} app_state_t;

app_state_t app_state_get(void);
void app_state_set(app_state_t app_state);
char *app_imei_get(void);
char *app_msisdn_get(void);

void app_request_server_update(uint16_t instance_id);
bool app_did_bootstrap(void);
uint16_t app_server_instance(void);
sa_family_t app_family_type_get(uint16_t instance_id);
int32_t app_state_update_delay(void);

void app_bootstrap_reset(void);
void app_factory_reset(void);
void app_system_shutdown(void);
void app_system_reset(void);
void app_request_reboot(void);

#endif // LWM2M_VZW_MAIN_H__
