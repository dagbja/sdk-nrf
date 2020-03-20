/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CARRIER_MAIN_H__

#include <stdint.h>
#include <stdbool.h>
#include <nrf_socket.h>

typedef enum
{
    LWM2M_STATE_BOOTING,
    LWM2M_STATE_IDLE,
    LWM2M_STATE_REQUEST_LINK_UP,
    LWM2M_STATE_REQUEST_LINK_DOWN,
    LWM2M_STATE_REQUEST_CONNECT,
    LWM2M_STATE_BS_HOLD_OFF,
    LWM2M_STATE_BS_CONNECT,
    LWM2M_STATE_BS_CONNECT_WAIT,
    LWM2M_STATE_BS_CONNECT_RETRY_WAIT,
    LWM2M_STATE_BS_CONNECTED,
    LWM2M_STATE_BOOTSTRAP_REQUESTED,
    LWM2M_STATE_BOOTSTRAP_WAIT,
    LWM2M_STATE_BOOTSTRAP_TIMEDOUT,
    LWM2M_STATE_BOOTSTRAPPING,
    LWM2M_STATE_CLIENT_HOLD_OFF,
    LWM2M_STATE_SERVER_CONNECT,
    LWM2M_STATE_SERVER_CONNECT_WAIT,
    LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT,
    LWM2M_STATE_SERVER_CONNECTED,
    LWM2M_STATE_SERVER_REGISTER_WAIT,
    LWM2M_STATE_SERVER_DEREGISTER,
    LWM2M_STATE_SERVER_DEREGISTERING,
    LWM2M_STATE_REQUEST_DISCONNECT,
    LWM2M_STATE_DISCONNECTED,
    LWM2M_STATE_MODEM_FIRMWARE_UPDATE,
    LWM2M_STATE_SHUTDOWN,
    LWM2M_STATE_RESET
} lwm2m_state_t;

lwm2m_state_t lwm2m_state_get(void);

/**
 * @brief Get the device client id.
 *
 * @return Client ID
 */
char *lwm2m_client_id_get(uint16_t *p_len);

/**
 * @brief Get device IMEI that we can use as a unique serial number.
 * @return IMEI has a NULL-terminated String.
 */
char *lwm2m_imei_get(void);

/**
 * @brief Get device MSISDN.
 * @return MSISDN has a NULL-terminated String.
 */
char *lwm2m_msisdn_get(void);

/**
 * @brief Get default APN.
 */
char *lwm2m_default_apn_get(void);

bool lwm2m_is_admin_pdn_ready(void);
bool lwm2m_admin_pdn_activate(uint16_t instance_id, int32_t *retry_delay);

void lwm2m_request_link_up(void);
void lwm2m_request_link_down(void);
void lwm2m_request_bootstrap(void);
void lwm2m_request_connect(void);
void lwm2m_request_server_update(uint16_t instance_id, bool reconnect);
bool lwm2m_request_remote_reconnect(struct nrf_sockaddr *p_remote);
void lwm2m_request_deregister(void);
void lwm2m_request_disconnect(void);
void lwm2m_request_reset(void);
bool lwm2m_did_bootstrap(void);
uint16_t lwm2m_security_instance(void);
nrf_sa_family_t lwm2m_family_type_get(uint16_t instance_id);
int32_t lwm2m_state_update_delay(void);

void lwm2m_bootstrap_clear(void);
void lwm2m_factory_reset(void);
void lwm2m_system_shutdown(void);
void lwm2m_system_reset(bool force_reset);

uint32_t lwm2m_net_reg_stat_get(void);
void lwm2m_net_reg_stat_cb(uint32_t net_stat);

#endif // LWM2M_CARRIER_MAIN_H__
