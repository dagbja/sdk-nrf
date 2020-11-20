/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CLIENT_UTIL_H__
#define LWM2M_CLIENT_UTIL_H__

#include <stdbool.h>
#include <stdint.h>

#include <nrf_socket.h>

void client_init_sockaddr_in(struct nrf_sockaddr_in6 * p_addr, struct nrf_sockaddr * p_src, nrf_sa_family_t ai_family, uint16_t port);

const char * client_parse_uri(char * p_uri, uint8_t uri_len, uint16_t * p_port, bool * p_secure);

#if CONFIG_NRF_LWM2M_ENABLE_LOGS || CONFIG_SHELL
const char *client_remote_ntop(struct nrf_sockaddr_in6 *p_addr);
#endif

#endif // LWM2M_CLIENT_UTIL__