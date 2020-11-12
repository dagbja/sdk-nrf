/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CARRIER_CLIENT_H__
#define LWM2M_CARRIER_CLIENT_H__

#include <stdint.h>

/**
 * @brief Initialize client internal settings.
 *
 * @note This is only done once.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_init(void);

/**
 * @brief Configure client instances.
 *
 * Configure client instances according to "is_bootstrapped" setting.
 *
 * @note This is done after loading objects from flash and after bootstrap
 *       to configure clients according to current object settings.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_configure(void);

/**
 * @brief Schedule connect to all configured clients.
 *
 * @note This is done when device is connected to home network.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_connect(void);

/**
 * @brief Bootstrap sequence is completed from server.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_bootstrap_done(void);

/**
 * @brief Schedule a client registration update.
 *
 * @param[in] server_instance LwM2M Server object instance
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_update(uint16_t server_instance);

/**
 * @brief Schedule a client disable.
 *
 * @param[in] server_instance LwM2M Server object instance
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_disable(uint16_t server_instance);

/**
 * @brief Schedule a client reconnect.
 *
 * @param[in] security_instance LwM2M Security object instance
 *
 * @note This is for handling the VzW DTLS fatal alert injection to
 *       force a client reconnection.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_reconnect(uint16_t security_instance);

/**
 * @brief Schedule disconnect all connected clients.
 *
 * @note This is done when device is not connected to home network.
 *
 * @return 0 on success, -errno on error.
 */
int lwm2m_client_disconnect(void);

#endif // LWM2M_CARRIER_CLIENT_H__