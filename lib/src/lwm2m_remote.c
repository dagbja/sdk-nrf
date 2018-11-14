/*$$$LICENCE_NORDIC_STANDARD<2016>$$$*/
#include <string.h>

#include "lwm2m.h"
#include "lwm2m_api.h"

#if LWM2M_CONFIG_LOG_ENABLED

#define NRF_LOG_MODULE_NAME lwm2m

#define NRF_LOG_LEVEL       LWM2M_CONFIG_LOG_LEVEL
#define NRF_LOG_INFO_COLOR  LWM2M_CONFIG_INFO_COLOR
#define NRF_LOG_DEBUG_COLOR LWM2M_CONFIG_DEBUG_COLOR

#include "nrf_log.h"

#define LWM2M_TRC     NRF_LOG_DEBUG                                                                 /**< Used for getting trace of execution in the module. */
#define LWM2M_ERR     NRF_LOG_ERROR                                                                 /**< Used for logging errors in the module. */
#define LWM2M_DUMP    NRF_LOG_HEXDUMP_DEBUG                                                         /**< Used for dumping octet information to get details of bond information etc. */

#define LWM2M_ENTRY() LWM2M_TRC(">> %s", __func__)
#define LWM2M_EXIT()  LWM2M_TRC("<< %s", __func__)

#else // LWM2M_CONFIG_LOG_ENABLED

#define LWM2M_TRC(...)                                                                              /**< Disables traces. */
#define LWM2M_DUMP(...)                                                                             /**< Disables dumping of octet streams. */
#define LWM2M_ERR(...)                                                                              /**< Disables error logs. */

#define LWM2M_ENTRY(...)
#define LWM2M_EXIT(...)

#endif // LWM2M_CONFIG_LOG_ENABLED

uint16_t             m_count;
uint16_t             m_short_server_id[LWM2M_MAX_SERVERS];
struct sockaddr_in6  m_remotes[LWM2M_MAX_SERVERS];
char                 m_location[LWM2M_MAX_SERVERS][LWM2M_REGISTER_MAX_LOCATION_LEN];
uint16_t             m_location_len[LWM2M_MAX_SERVERS];


#define LWM2M_REMOTE_FIND_OR_RETURN_ERR(ID, IDX) \
    IDX = find_index(ID);                        \
    if (IDX < 0)                                 \
    {                                            \
       return LWM2M_ERROR(NRF_ERROR_NOT_FOUND);  \
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
    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_register(uint16_t short_server_id)
{
    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_register. SSID: %u.\r\n", short_server_id);

    int index = find_index(short_server_id);

    if (index < 0)
    {
        if (m_count + 1 > LWM2M_MAX_SERVERS)
        {
            return LWM2M_ERROR(NRF_ERROR_NO_MEM);
        }

        m_short_server_id[m_count] = short_server_id;
        m_count++;
    }

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_register.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_deregister(uint16_t short_server_id)
{
    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_deregister. SSID: %u.\r\n", short_server_id);

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
    memset(&m_remotes[m_count], 0, sizeof(struct sockaddr_in6));
    memset(m_location[m_count], 0, LWM2M_REGISTER_MAX_LOCATION_LEN);
    m_location_len[m_count] = 0;

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_deregister.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_remote_save(struct sockaddr * p_remote, uint16_t short_server_id)
{
    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_remote_save. SSID: %u.\r\n", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    const struct sockaddr_in  * p_remote4 = (struct sockaddr_in *)p_remote;
    const struct sockaddr_in6 * p_remote6 = (struct sockaddr_in6 *)p_remote;

    if (p_remote->sa_family == AF_INET6)
    {
       memcpy(&m_remotes[index], p_remote6, sizeof(struct sockaddr_in6));
    }
    else
    {
       memcpy(&m_remotes[index], p_remote4, sizeof(struct sockaddr_in));
    }

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_remote_save.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_short_server_id_find(uint16_t        * p_short_server_id,
                                           struct sockaddr * p_remote)
{
    NULL_PARAM_CHECK(p_short_server_id)

    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_short_server_id_find.\r\n");

    const struct sockaddr_in  * p_remote4 = (struct sockaddr_in *)p_remote;
    const struct sockaddr_in6 * p_remote6 = (struct sockaddr_in6 *)p_remote;

    for (int i = 0; i < m_count; ++i)
    {
        if (p_remote->sa_family == AF_INET6)
        {
            if (memcmp(&m_remotes[i], p_remote6, sizeof(struct sockaddr_in6)) == 0)
            {
                *p_short_server_id = m_short_server_id[i];
                LWM2M_TRC("[Remote  ]: << lwm2m_remote_short_server_id_find. Found: %u\r\n",
                          m_short_server_id[i]);
                return NRF_SUCCESS;
            }
        }

        if (p_remote->sa_family == AF_INET)
        {
            if (memcmp(&m_remotes[i], p_remote4, sizeof(struct sockaddr_in)) == 0)
            {
                *p_short_server_id = m_short_server_id[i];
                LWM2M_TRC("[Remote  ]: << lwm2m_remote_short_server_id_find. Found: %u\r\n",
                          m_short_server_id[i]);
                return NRF_SUCCESS;
            }
        }
    }

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_short_server_id_find. Not Found.\r\n");

    return LWM2M_ERROR(NRF_ERROR_NOT_FOUND);
}


uint32_t lwm2m_short_server_id_remote_find(struct sockaddr ** pp_remote,
                                           uint16_t           short_server_id)
{
    NULL_PARAM_CHECK(pp_remote)

    LWM2M_TRC("[Remote  ]: >> lwm2m_short_server_id_remote_find. SSID: %u.\r\n",
              short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    *pp_remote = (struct sockaddr *)&m_remotes[index];

    LWM2M_TRC("[Remote  ]: << lwm2m_short_server_id_remote_find.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_location_save(char   * p_location,
                                    uint16_t location_len,
                                    uint16_t short_server_id)
{
    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_location_save. SSID: %u.\r\n", short_server_id);

    if (location_len > LWM2M_REGISTER_MAX_LOCATION_LEN)
    {
        return LWM2M_ERROR(NRF_ERROR_NO_MEM);
    }

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    memcpy(m_location[index], p_location, location_len);
    m_location_len[index] = location_len;

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_location_save.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_remote_location_find(char    ** pp_location,
                                    uint16_t * p_location_len,
                                    uint16_t   short_server_id)
{
    NULL_PARAM_CHECK(pp_location)
    NULL_PARAM_CHECK(p_location_len)

    LWM2M_TRC("[Remote  ]: >> lwm2m_remote_location_find. SSID: %u.\r\n", short_server_id);

    int index;
    LWM2M_REMOTE_FIND_OR_RETURN_ERR(short_server_id, index)

    *pp_location = m_location[index];
    *p_location_len = m_location_len[index];

    LWM2M_TRC("[Remote  ]: << lwm2m_remote_location_find.\r\n");

    return NRF_SUCCESS;
}
