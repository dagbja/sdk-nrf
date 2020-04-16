/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>

#include <nrf_socket.h>
#include <lwm2m.h>
#include <lwm2m_api.h>

static struct nrf_sockaddr_in6  m_remotes[LWM2M_MAX_SERVERS];
static bool                     m_reconnecting[LWM2M_MAX_SERVERS];

static struct __attribute__((__packed__)) {
    uint16_t ssid;
    uint16_t len;
    char data[LWM2M_REGISTER_MAX_LOCATION_LEN];
} m_location[LWM2M_MAX_SERVERS];


#define LWM2M_REMOTE_FIND_OR_RETURN_ERR(ID, IDX) \
    IDX = find_index(ID);                        \
    if (IDX < 0)                                 \
    {                                            \
       return ENOENT;                            \
    }


static int find_index(uint16_t short_server_id)
{
    for (int i = 0; i < LWM2M_MAX_SERVERS; ++i)
    {
        if (m_location[i].ssid == short_server_id)
        {
            return i;
        }
    }

    return -1;
}

static int find_free(void)
{
    for (int i = 0; i < LWM2M_MAX_SERVERS; ++i)
    {
        if (m_location[i].ssid == 0)
        {
            return i;
        }
    }

    return -1;
}


uint32_t lwm2m_remote_init()
{
    for (int i = 0; i < LWM2M_MAX_SERVERS; i++)
    {
        m_location[i].ssid = 0;
    }

    return 0;
}

void lwm2m_remote_location_get(void **pp_remotes, size_t *p_len)
{
    __ASSERT_NO_MSG(pp_remotes != NULL);
    __ASSERT_NO_MSG(p_len != NULL);

    *pp_remotes = m_location;
    *p_len = sizeof(m_location);
}

uint32_t lwm2m_remote_register(uint16_t short_server_id, struct nrf_sockaddr * p_remote)
{
    int index;

    index = find_index(short_server_id);
    if (index < 0)
    {
        index = find_free();
    }
    if (index < 0)
    {
        return ENOMEM;
    }

    m_location[index].ssid = short_server_id;
    LWM2M_TRC("Server registered, ssid %d", short_server_id);

    if (p_remote->sa_family == NRF_AF_INET6)
    {
        const struct nrf_sockaddr_in6 * p_remote6 = (struct nrf_sockaddr_in6 *)p_remote;
        memcpy(&m_remotes[index], p_remote6, sizeof(struct nrf_sockaddr_in6));
    }
    else
    {
        const struct nrf_sockaddr_in  * p_remote4 = (struct nrf_sockaddr_in *)p_remote;
        memcpy(&m_remotes[index], p_remote4, sizeof(struct nrf_sockaddr_in));
    }

    return 0;
}

uint32_t lwm2m_remote_deregister(uint16_t short_server_id)
{
    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    LWM2M_TRC("Server deregistered, ssdi: %d", short_server_id);

    // Clear out the last index.
    m_location[index].ssid = 0;
    m_location[index].len    = 0;
    memset(&m_remotes[index], 0, sizeof(struct nrf_sockaddr_in6));
    memset(m_location[index].data, 0, LWM2M_REGISTER_MAX_LOCATION_LEN);

    LWM2M_EXIT();

    return 0;
}

bool lwm2m_remote_is_registered(uint16_t short_server_id)
{
    for (size_t i = 0; i < LWM2M_MAX_SERVERS; i++) {
        if (m_location[i].ssid == short_server_id) {
            return true;
        }
    }
    return false;
}

uint32_t lwm2m_remote_short_server_id_find(uint16_t            * p_short_server_id,
                                           struct nrf_sockaddr * p_remote)
{
    NULL_PARAM_CHECK(p_short_server_id)

    LWM2M_ENTRY();

    const struct nrf_sockaddr_in  * p_remote4 = (struct nrf_sockaddr_in *)p_remote;
    const struct nrf_sockaddr_in6 * p_remote6 = (struct nrf_sockaddr_in6 *)p_remote;

    for (int i = 0; i < LWM2M_MAX_SERVERS; ++i)
    {
        if (p_remote->sa_family == NRF_AF_INET6)
        {
            if (memcmp(&m_remotes[i], p_remote6, sizeof(struct nrf_sockaddr_in6)) == 0)
            {
                *p_short_server_id = m_location[i].ssid;
                LWM2M_TRC("Found: %u", m_location[i].ssid);
                return 0;
            }
        }

        if (p_remote->sa_family == NRF_AF_INET)
        {
            if (memcmp(&m_remotes[i], p_remote4, sizeof(struct nrf_sockaddr_in)) == 0)
            {
                *p_short_server_id = m_location[i].ssid;
                LWM2M_TRC("Found: %u", m_location[i].ssid);
                return 0;
            }
        }
    }

    LWM2M_TRC("Not Found");

    return ENOENT;
}


uint32_t lwm2m_short_server_id_remote_find(nrf_sockaddr ** pp_remote,
                                           uint16_t        short_server_id)
{
    NULL_PARAM_CHECK(pp_remote)

    LWM2M_TRC("SSID: %u", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    *pp_remote = (struct nrf_sockaddr *)&m_remotes[index];

    LWM2M_EXIT();

    return 0;
}


uint32_t lwm2m_remote_location_save(char   * p_location,
                                    uint16_t location_len,
                                    uint16_t short_server_id)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    if (location_len > LWM2M_REGISTER_MAX_LOCATION_LEN)
    {
        return ENOMEM;
    }

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    memcpy(m_location[index].data, p_location, location_len);
    m_location[index].len = location_len;

    LWM2M_EXIT();

    return 0;
}


uint32_t lwm2m_remote_location_clear(void)
{
    memset(m_location, 0, sizeof(m_location));

    return 0;
}


uint32_t lwm2m_remote_location_find(char    ** pp_location,
                                    uint16_t * p_location_len,
                                    uint16_t   short_server_id)
{
    NULL_PARAM_CHECK(pp_location)
    NULL_PARAM_CHECK(p_location_len)

    LWM2M_TRC("SSID: %u", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    *pp_location = m_location[index].data;
    *p_location_len = m_location[index].len;

    LWM2M_EXIT();

    return 0;
}

int lwm2m_remote_reconnecting_set(uint16_t short_server_id)
{
    int index;

    index = find_index(short_server_id);
    if (index < 0) {
        return -ENOENT;
    }

    m_reconnecting[index] = true;

    return 0;
}

bool lwm2m_remote_reconnecting_get(uint16_t short_server_id)
{
    int index;

    index = find_index(short_server_id);
    if (index < 0) {
        return false;
    }

    return m_reconnecting[index];
}

int lwm2m_remote_reconnecting_clear(uint16_t short_server_id)
{
    int index;

    index = find_index(short_server_id);
    if (index < 0) {
        return -ENOENT;
    }

    m_reconnecting[index] = false;

    return 0;
}
