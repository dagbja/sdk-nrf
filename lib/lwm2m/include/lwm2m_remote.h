/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file lwm2m_remote.h
 *
 * @defgroup iot_sdk_lwm2m_remote_api LWM2M Remote API interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief Remote API interface for the LWM2M protocol.
 */

#ifndef LWM2M_REMOTE_H__
#define LWM2M_REMOTE_H__

#include <stdint.h>
#include <nrf_socket.h>

/**
 * @brief      Initialize this module.
 * @details    Calling this method will set the module in its default state.
 *
 * @return     NRF_SUCCESS Initialization succeeded.
 */
uint32_t lwm2m_remote_init(void);

/**
 * @brief      Register a new short server id. Must be used before lwm2m_remote_remote_save.
 *
 * @param[in]  short_server_id  Short server id associated with Server and Security instances. The value MUST correspond
 *                              with the short server ids used for Server and Security instances.
 * @param[in]  p_remote         Remote to save.
 *
 * @return     NRF_SUCCESS      Registration succeeded.
 * @return     NRF_ERROR_NO_MEM The remote list is full.
 */
uint32_t lwm2m_remote_register(uint16_t short_server_id, struct nrf_sockaddr * p_remote);

/**
 * @brief      Deregister this short server id.
 *
 * @param[in]  short_server_id  Short server id associated with this remote. The value MUST correspond
 *                              with the short server ids used for Server and Security instances.
 *
 * @return     NRF_SUCCESS         Deregistration succeeded.
 * @return     NRF_ERROR_NOT_FOUND The short_server_id was not found.
 */
uint32_t lwm2m_remote_deregister(uint16_t short_server_id);

/**
 * @brief      Find the short server id from a given remote struct.
 *
 * @param[out] p_short_server_id Short server id found.
 * @param[in]  p_remote          The remote to look for.
 *
 * @return     NRF_SUCCESS         The short server id was retrieved.
 * @return     NRF_ERROR_NOT_FOUND The struct nrf_sockaddr was not found.
 */
uint32_t lwm2m_remote_short_server_id_find(uint16_t       * p_short_server_id,
                                           struct nrf_sockaddr * p_remote);

/**
 * @brief      Find the struct nrf_sockaddr based on short_server_id.
 *
 * @param[out] pp_remote        The pointer to the remote found.
 * @param[in]  short_server_id  The short_server_id to look for.
 *
 * @return     NRF_SUCCESS         The remote was retrieved.
 * @return     NRF_ERROR_NOT_FOUND The short_server_id was not found.
 */
uint32_t lwm2m_short_server_id_remote_find(struct nrf_sockaddr ** pp_remote,
                                           uint16_t          short_server_id);

/**
 * @brief      Associate a location with a short server id.
 *
 * @param[in]  p_location       Location string to save.
 * @param[in]  location_len     Length of the location.
 * @param[in]  short_server_id  Short server id to associate with.
 *
 * @return     NRF_SUCCESS         The location was saved.
 * @return     NRF_ERROR_NOT_FOUND The short_server_id was not found.
 * @return     NRF_ERROR_NO_MEM    The location string was too long.
 */
uint32_t lwm2m_remote_location_save(char     * p_location,
                                    uint16_t   location_len,
                                    uint16_t   short_server_id);


/**
 * @brief      Delete a location associated with a short server id.
 *
 * @param[in]  short_server_id  Short server id to associate with.
 *
 * @return     NRF_SUCCESS         The location was deleted.
 * @return     NRF_ERROR_NOT_FOUND The short_server_id was not found.
 */
uint32_t lwm2m_remote_location_delete(uint16_t short_server_id);

/**
 * @brief      Find the location associated with a given remote.
 *
 * @details    Find the location associated with a given short server id. If no allocation has been
 *             done with the given short server id, it returns NRF_ERROR_NOT_FOUND. If no
 *             location has been saved with this server id yet, the location_len will be zero and the
 *             location pointer will be invalid.
 *
 * @param[out] pp_location      Location found.
 * @param[out] p_location_len   Length of found location.
 * @param[in]  short_server_id  Short server id to find location of.
 *
 * @return     NRF_SUCCESS         The location was retrieved.
 * @return     NRF_ERROR_NOT_FOUND The short_server_id was not found.
 */
uint32_t lwm2m_remote_location_find(char     ** pp_location,
                                    uint16_t  * p_location_len,
                                    uint16_t    short_server_id);


int lwm2m_remote_reconnecting_set(uint16_t short_server_id);
bool lwm2m_remote_reconnecting_get(uint16_t short_server_id);
int lwm2m_remote_reconnecting_clear(uint16_t short_server_id);


#endif // LWM2M_REMOTE_H__

/**@} */
