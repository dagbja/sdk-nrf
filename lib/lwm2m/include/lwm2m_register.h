/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m_register.h
 *
 * @defgroup iot_sdk_lwm2m_register_api LWM2M register API interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief Register API interface for the LWM2M protocol.
 */

#ifndef LWM2M_REGISTER_H__
#define LWM2M_REGISTER_H__

#include <stdint.h>
#include <net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Initialize the LWM2M register module.
 *
 * @details Calling this function will set the module in default state.
 */
uint32_t internal_lwm2m_register_init(void);

/**
 * @brief      Get the short_server_id from a remote.
 *
 * @param[out]  p_ssi       the short server id matching the remote.
 * @param[in]   p_remote    remote to lookup short id of.
 *
 * @return     NRF_SUCCESS / NRF_NOT_FOUND
 */
uint32_t internal_lwm2m_short_server_id_lookup(uint16_t * p_ssi, struct sockaddr * p_remote);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_REGISTER_H__

/**@} */
