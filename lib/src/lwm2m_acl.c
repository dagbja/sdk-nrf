/*$$$LICENCE_NORDIC_STANDARD<2016>$$$*/
#include <string.h>

#include "lwm2m_acl.h"
#include "lwm2m_tlv.h"
#include "lwm2m_objects.h"

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

#define LWM2M_ACL_INTERNAL_NOT_FOUND      65537

uint16_t m_index_counter;


/**
 * @brief      Find the index of a specific short_server_id.
 *
 * @details    Passing a short_server_id of 0 will find
 *             first available index. NOTE: This function skips
 *             the first index of the server array as this is reserved
 *             for default acl. So default lookup must be checked before
 *             calling this function.
 *
 * @param[in]       servers               Array of server to search in.
 * @param[in]       short_server_id       Short server id to find.
 *
 * @return     index                            If found.
 * @return     LWM2M_ACL_INTERNAL_NOT_FOUND     If not found.
 */
static uint32_t index_find(uint16_t servers[1+LWM2M_MAX_SERVERS], uint16_t short_server_id)
{
    // Index 0 is reserved for default. And is checked outside of this function.
    // See LWM2M Spec Table 36: Access Control Object [3] (for the Connectivity Monitoring Object).
    for (int i = 1; i < (1+LWM2M_MAX_SERVERS); ++i)
    {
        if (servers[i] == short_server_id)
        {
            return i;
        }
    }

    return LWM2M_ACL_INTERNAL_NOT_FOUND;
}


static void index_buffer_len_update(uint32_t * index, uint32_t * buffer_len, uint32_t max_buffer)
{
    *index     += *buffer_len;
    *buffer_len = max_buffer - *index;
}


uint32_t lwm2m_acl_init(void)
{
    m_index_counter = 0;
    return NRF_SUCCESS;
}


uint32_t lwm2m_acl_permissions_init(lwm2m_instance_t * p_instance,
                                    uint16_t           owner)
{
    NULL_PARAM_CHECK(p_instance);

    memset(p_instance->acl.access, 0, sizeof(uint16_t) * (1+LWM2M_MAX_SERVERS));
    memset(p_instance->acl.server, 0, sizeof(uint16_t) * (1+LWM2M_MAX_SERVERS));

    p_instance->acl.id        = m_index_counter;
    p_instance->acl.owner     = owner;
    m_index_counter++;

    return NRF_SUCCESS;
}


uint32_t lwm2m_acl_permissions_check(uint16_t         * p_access,
                                     lwm2m_instance_t * p_instance,
                                     uint16_t           short_server_id)
{
    LWM2M_TRC("[ACL  ]: >> lwm2m_acl_permissions_check. SSID: %u.\r\n", short_server_id);

    NULL_PARAM_CHECK(p_instance);

    // Owner has full access
    if (short_server_id == p_instance->acl.owner)
    {
        *p_access = LWM2M_ACL_FULL_PERM;

        LWM2M_TRC("[ACL  ]: << lwm2m_acl_permissions_check. %u is owner.\r\n", short_server_id);

        return NRF_SUCCESS;
    }

    // Find index
    uint32_t index;
    if (short_server_id == LWM2M_ACL_DEFAULT_SHORT_SERVER_ID)
    {
        index = 0;
    }
    else
    {
        index = index_find(p_instance->acl.server, short_server_id);

        if (index == LWM2M_ACL_INTERNAL_NOT_FOUND)
        {
            // Set access to LWM2M_ACL_NO_PERM in case of no error checking.
            *p_access = LWM2M_ACL_NO_PERM;

            LWM2M_TRC("[ACL  ]: << lwm2m_acl_permissions_check. %u was not found.\r\n", short_server_id);

            return LWM2M_ERROR(NRF_ERROR_NOT_FOUND);
        }
    }

    *p_access = p_instance->acl.access[index];

    LWM2M_TRC("[ACL  ]: << lwm2m_acl_permissions_check. Success.\r\n");

    return NRF_SUCCESS;
}


uint32_t lwm2m_acl_permissions_add(lwm2m_instance_t * p_instance,
                                   uint16_t           access,
                                   uint16_t           short_server_id)
{
    NULL_PARAM_CHECK(p_instance);

    // Find index
    uint32_t index;
    if (short_server_id == LWM2M_ACL_DEFAULT_SHORT_SERVER_ID)
    {
        index = 0;
    }
    else
    {
        // Find first free index by passing 0 as short_server_id
        index = index_find(p_instance->acl.server, 0);

        if (index == LWM2M_ACL_INTERNAL_NOT_FOUND)
        {
            return LWM2M_ERROR(NRF_ERROR_NO_MEM);
        }
    }

    p_instance->acl.access[index] = access;
    p_instance->acl.server[index] = short_server_id;

    return NRF_SUCCESS;
}


uint32_t lwm2m_acl_permissions_remove(lwm2m_instance_t * p_instance,
                                      uint16_t           short_server_id)
{
    NULL_PARAM_CHECK(p_instance);

    // Find index
    uint32_t index;
    if (short_server_id == LWM2M_ACL_DEFAULT_SHORT_SERVER_ID)
    {
        index = 0;
    }
    else
    {
        index = index_find(p_instance->acl.server, short_server_id);

        if (index == LWM2M_ACL_INTERNAL_NOT_FOUND)
        {
            return LWM2M_ERROR(NRF_ERROR_NOT_FOUND);
        }
    }

    p_instance->acl.server[index] = 0;
    p_instance->acl.access[index] = 0;

    return NRF_SUCCESS;
}


uint32_t lwm2m_acl_serialize_tlv(uint8_t          * p_buffer,
                                 uint32_t         * p_buffer_len,
                                 lwm2m_instance_t * p_instance)
{
    uint32_t err_code;
    uint32_t max_buffer = *p_buffer_len;
    uint32_t index      = 0;

    // Encode the Object ID
    err_code = lwm2m_tlv_integer_encode(p_buffer + index, p_buffer_len,
                                        LWM2M_ACL_OBJECT_ID,
                                        p_instance->object_id);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode the Instance ID
    err_code = lwm2m_tlv_integer_encode(p_buffer + index, p_buffer_len,
                                        LWM2M_ACL_INSTANCE_ID,
                                        p_instance->instance_id);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode ACLs
    lwm2m_list_t list;
    uint16_t list_identifiers[LWM2M_MAX_SERVERS];
    uint16_t list_values[LWM2M_MAX_SERVERS];

    list.type         = LWM2M_LIST_TYPE_UINT16;
    list.p_id         = list_identifiers;
    list.val.p_uint16 = list_values;
    list.len          = 0;

    for (int i = 1; i < (1+LWM2M_MAX_SERVERS); ++i)
    {
        if (p_instance->acl.server[i] != 0)
        {
            list_identifiers[list.len] = p_instance->acl.server[i];
            list_values[list.len]      = p_instance->acl.access[i];
            list.len++;
        }
    }

    err_code = lwm2m_tlv_list_encode(p_buffer + index, p_buffer_len, LWM2M_ACL_ACL, &list);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode owner.
    err_code = lwm2m_tlv_integer_encode(p_buffer + index, p_buffer_len,
                                        LWM2M_ACL_CONTROL_OWNER,
                                        p_instance->acl.owner);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    *p_buffer_len = index;

    return NRF_SUCCESS;
}
