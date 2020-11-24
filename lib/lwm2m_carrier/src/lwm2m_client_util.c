/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <zephyr.h>    /* For __ASSERT */
#include <lwm2m_os.h>  /* For lwm2m_os_errno() */
#include <nrf_errno.h> /* NRF_Exxx */
#include <lwm2m_client_util.h>

#define URI_SCHEME_COAPS "coaps://"
#define URI_SCHEME_COAP "coap://"

void client_init_sockaddr_in(struct nrf_sockaddr_in6 *p_addr,
                             struct nrf_sockaddr *p_src,
                             nrf_sa_family_t ai_family, uint16_t port)
{
    __ASSERT(p_addr != NULL, "Invalid argument");

    memset(p_addr, 0, sizeof(struct nrf_sockaddr_in6));

    if (ai_family == NRF_AF_INET) {
        struct nrf_sockaddr_in *addr_in = (struct nrf_sockaddr_in *)p_addr;

        addr_in->sin_len = sizeof(struct nrf_sockaddr_in);
        addr_in->sin_family = ai_family;
        addr_in->sin_port = nrf_htons(port);

        if (p_src) {
            addr_in->sin_addr.s_addr =
                ((struct nrf_sockaddr_in *)p_src)->sin_addr.s_addr;
        }
    } else {
        struct nrf_sockaddr_in6 *addr_in6 = (struct nrf_sockaddr_in6 *)p_addr;

        addr_in6->sin6_len = sizeof(struct nrf_sockaddr_in6);
        addr_in6->sin6_family = ai_family;
        addr_in6->sin6_port = nrf_htons(port);

        if (p_src) {
            memcpy(addr_in6->sin6_addr.s6_addr,
                   ((struct nrf_sockaddr_in6 *)p_src)->sin6_addr.s6_addr, 16);
        }
    }
}

const char *client_parse_uri(char *p_uri, uint8_t uri_len, uint16_t *p_port,
                             bool *p_secure)
{
    const char *p_hostname;

    __ASSERT(p_uri != NULL && uri_len != 0 && p_port != NULL &&
                 p_secure != NULL,
             "Invalid argument");

    if (strncmp(p_uri, URI_SCHEME_COAPS, sizeof(URI_SCHEME_COAPS) - 1) == 0) {
        p_hostname = &p_uri[8];
        *p_port = 5684;
        *p_secure = true;
    } else if (strncmp(p_uri, URI_SCHEME_COAP, sizeof(URI_SCHEME_COAP) - 1) ==
               0) {
        p_hostname = &p_uri[7];
        *p_port = 5683;
        *p_secure = false;
    } else {
        /* Unknown scheme. */
        return NULL;
    }

    char *sep = strchr(p_hostname, ':');
    if (sep) {
        *sep = '\0';
        *p_port = atoi(sep + 1);
    }

    return p_hostname;
}

int lwm2m_client_errno(uint32_t err_code)
{
    switch (err_code) {
    case NRF_EPERM:
        return EPERM;
    case NRF_ENOENT:
        return ENOENT;
    case NRF_EIO:
        return EIO;
    case NRF_ENOEXEC:
        return ENOEXEC;
    case NRF_EBADF:
        return EBADF;
    case NRF_ENOMEM:
        return ENOMEM;
    case NRF_EACCES:
        return EACCES;
    case NRF_EFAULT:
        return EFAULT;
    case NRF_EINVAL:
        return EINVAL;
    case NRF_EMFILE:
        return EMFILE;
    case NRF_EAGAIN:
        return EAGAIN;
    case NRF_EDOM:
        return EDOM;
    case NRF_EPROTOTYPE:
        return EPROTOTYPE;
    case NRF_ENOPROTOOPT:
        return ENOPROTOOPT;
    case NRF_EPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case NRF_ESOCKTNOSUPPORT:
        return ESOCKTNOSUPPORT;
    case NRF_EOPNOTSUPP:
        return EOPNOTSUPP;
    case NRF_EAFNOSUPPORT:
        return EAFNOSUPPORT;
    case NRF_EADDRINUSE:
        return EADDRINUSE;
    case NRF_ENETDOWN:
        return ENETDOWN;
    case NRF_ENETUNREACH:
        return ENETUNREACH;
    case NRF_ENETRESET:
        return ENETRESET;
    case NRF_ECONNRESET:
        return ECONNRESET;
    case NRF_EISCONN:
        return EISCONN;
    case NRF_ENOTCONN:
        return ENOTCONN;
    case NRF_ETIMEDOUT:
        return ETIMEDOUT;
    case NRF_ENOBUFS:
        return ENOBUFS;
    case NRF_EHOSTDOWN:
        return EHOSTDOWN;
    case NRF_EINPROGRESS:
        return EINPROGRESS;
    case NRF_EALREADY:
        return EALREADY;
    case NRF_ECANCELED:
        return ECANCELED;
    case NRF_EMSGSIZE:
        return EMSGSIZE;
    case NRF_ECONNABORTED:
        return ECONNABORTED;
    default:
        /* Invalid error code */
        return EINVAL;
    }
}

#if CONFIG_NRF_LWM2M_ENABLE_LOGS || CONFIG_SHELL
const char *client_remote_ntop(struct nrf_sockaddr_in6 *p_remote)
{
    static char ip_buffer[64];
    void *p_addr = NULL;

    if (p_remote->sin6_family == NRF_AF_INET6) {
        p_addr = &p_remote->sin6_addr.s6_addr;
    } else if (p_remote->sin6_family == NRF_AF_INET) {
        p_addr = &((struct nrf_sockaddr_in *)p_remote)->sin_addr.s_addr;
    }

    if (p_addr) {
        nrf_inet_ntop(p_remote->sin6_family, p_addr, ip_buffer,
                      sizeof(ip_buffer));
    } else {
        strcpy(ip_buffer, "<none>");
    }

    return ip_buffer;
}
#endif