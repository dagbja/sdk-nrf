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
    LWM2M_STATE_REQUEST_DISCONNECT,
    LWM2M_STATE_DISCONNECTED,
    LWM2M_STATE_MODEM_FIRMWARE_UPDATE,
    LWM2M_STATE_SHUTDOWN,
    LWM2M_STATE_RESET,
    LWM2M_STATE_ERROR
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
 * @brief Get carrier APN.
 *
 * @param[out] buf Buffer to store the APN into.
 * @param len Size of the buffer.
 * @return Length of the APN.
 */
int lwm2m_carrier_apn_get(char *buf, size_t len);

/** @brief Acknowledge SMS in library thread. */
void lwm2m_acknowledge_sms(void);

void lwm2m_request_link_up(void);
void lwm2m_request_link_down(void);
void lwm2m_request_bootstrap(void);
void lwm2m_request_connect(void);
bool lwm2m_request_remote_reconnect(struct nrf_sockaddr *p_remote);
void lwm2m_request_disconnect(void);
void lwm2m_request_reset(void);
bool lwm2m_did_bootstrap(void);
int32_t lwm2m_state_update_delay(void);

void lwm2m_main_bootstrap_reset(void);
void lwm2m_main_bootstrap_done(void);
int lwm2m_main_event_notify(uint32_t type, void * data);
int lwm2m_main_event_error(uint32_t error_code, int32_t error_value);

void lwm2m_set_bootstrapped(bool bootstrapped);
void lwm2m_bootstrap_clear(void);
void lwm2m_factory_reset(void);
void lwm2m_system_shutdown(void);
void lwm2m_system_reset(bool force_reset);

uint32_t lwm2m_net_reg_stat_get(void);
void lwm2m_net_reg_stat_cb(uint32_t net_stat);

#endif // LWM2M_CARRIER_MAIN_H__
