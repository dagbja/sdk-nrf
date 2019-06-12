/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>

#include <nrf_socket.h>
#include <lwm2m.h>
#include <lwm2m_api.h>

uint16_t             m_count;
uint16_t             m_short_server_id[LWM2M_MAX_SERVERS];
struct nrf_sockaddr_in6  m_remotes[LWM2M_MAX_SERVERS];
char                 m_location[LWM2M_MAX_SERVERS][LWM2M_REGISTER_MAX_LOCATION_LEN];
uint16_t             m_location_len[LWM2M_MAX_SERVERS];


#define LWM2M_REMOTE_FIND_OR_RETURN_ERR(ID, IDX) \
    IDX = find_index(ID);                        \
    if (IDX < 0)                                 \
    {                                            \
       return ENOENT;                            \
    }


static int find_index(uint16_t short_server_id)
{
    for (int i = 0; i < m_count; ++i)
    {
        if (m_short_server_id[i] == short_server_id)
        {
            return i;
        }
    }

    return -1;
}


uint32_t lwm2m_remote_init()
{
    m_count = 0;
    return 0;
}


uint32_t lwm2m_remote_register(uint16_t short_server_id, struct nrf_sockaddr * p_remote)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    int index = find_index(short_server_id);

    if (index < 0)
    {
        if (m_count + 1 > LWM2M_MAX_SERVERS)
        {
            return ENOMEM;
        }

        m_short_server_id[m_count] = short_server_id;

        if (p_remote->sa_family == NRF_AF_INET6)
        {
            const struct nrf_sockaddr_in6 * p_remote6 = (struct nrf_sockaddr_in6 *)p_remote;
            memcpy(&m_remotes[m_count], p_remote6, sizeof(struct nrf_sockaddr_in6));
        }
        else
        {
            const struct nrf_sockaddr_in  * p_remote4 = (struct nrf_sockaddr_in *)p_remote;
            memcpy(&m_remotes[m_count], p_remote4, sizeof(struct nrf_sockaddr_in));
        }

        m_count++;
    }

    LWM2M_EXIT();

    return 0;
}


uint32_t lwm2m_remote_deregister(uint16_t short_server_id)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    m_count--;

    // Move last element to the deleted index.
    m_short_server_id[index] = m_short_server_id[m_count];
    m_remotes[index] = m_remotes[m_count];
    memcpy(m_location[index], m_location[m_count], m_location_len[m_count]);
    m_location_len[index] = m_location_len[m_count];

    // Clear out the last index.
    m_short_server_id[m_count] = 0;
    memset(&m_remotes[m_count], 0, sizeof(struct nrf_sockaddr_in6));
    memset(m_location[m_count], 0, LWM2M_REGISTER_MAX_LOCATION_LEN);
    m_location_len[m_count] = 0;

    LWM2M_EXIT();

    return 0;
}


uint32_t lwm2m_remote_short_server_id_find(uint16_t        * p_short_server_id,
                                           struct nrf_sockaddr * p_remote)
{
    NULL_PARAM_CHECK(p_short_server_id)

    LWM2M_ENTRY();

    const struct nrf_sockaddr_in  * p_remote4 = (struct nrf_sockaddr_in *)p_remote;
    const struct nrf_sockaddr_in6 * p_remote6 = (struct nrf_sockaddr_in6 *)p_remote;

    for (int i = 0; i < m_count; ++i)
    {
        if (p_remote->sa_family == NRF_AF_INET6)
        {
            if (memcmp(&m_remotes[i], p_remote6, sizeof(struct nrf_sockaddr_in6)) == 0)
            {
                *p_short_server_id = m_short_server_id[i];
                LWM2M_TRC("Found: %u", m_short_server_id[i]);
                return 0;
            }
        }

        if (p_remote->sa_family == NRF_AF_INET)
        {
            if (memcmp(&m_remotes[i], p_remote4, sizeof(struct nrf_sockaddr_in)) == 0)
            {
                *p_short_server_id = m_short_server_id[i];
                LWM2M_TRC("Found: %u", m_short_server_id[i]);
                return 0;
            }
        }
    }

    LWM2M_TRC("Not Found");

    return ENOENT;
}


uint32_t lwm2m_short_server_id_remote_find(struct nrf_sockaddr ** pp_remote,
                                           uint16_t           short_server_id)
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

    memcpy(m_location[index], p_location, location_len);
    m_location_len[index] = location_len;

    LWM2M_EXIT();

    return 0;
}


uint32_t lwm2m_remote_location_delete(uint16_t short_server_id)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    m_location_len[index] = 0;

    LWM2M_EXIT();

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

    *pp_location = m_location[index];
    *p_location_len = m_location_len[index];

    LWM2M_EXIT();

    return 0;
}
