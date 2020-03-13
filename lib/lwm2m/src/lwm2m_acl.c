/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>

#include <lwm2m.h>
#include <lwm2m_acl.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>

#define LWM2M_ACL_INTERNAL_NOT_FOUND 65537

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
    return 0;
}


uint32_t lwm2m_acl_permissions_init(lwm2m_instance_t * p_instance,
                                    uint16_t           owner)
{
    NULL_PARAM_CHECK(p_instance);

    p_instance->acl.id = m_index_counter;
    m_index_counter++;

    return lwm2m_acl_permissions_reset(p_instance, owner);
}


uint32_t lwm2m_acl_permissions_check(uint16_t         * p_access,
                                     lwm2m_instance_t * p_instance,
                                     uint16_t           short_server_id)
{
    LWM2M_TRC("SSID: %u", short_server_id);

    NULL_PARAM_CHECK(p_instance);

    // Owner has full access
    if (short_server_id == p_instance->acl.owner)
    {
        *p_access = LWM2M_ACL_FULL_PERM;

        LWM2M_TRC("%u is owner", short_server_id);

        return 0;
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

            LWM2M_TRC("%u was not found", short_server_id);

            return ENOENT;
        }
    }

    *p_access = p_instance->acl.access[index];

    LWM2M_TRC("Success");

    return 0;
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
            return ENOMEM;
        }
    }

    p_instance->acl.access[index] = access;
    p_instance->acl.server[index] = short_server_id;

    return 0;
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
            return ENOENT;
        }
    }

    p_instance->acl.server[index] = 0;
    p_instance->acl.access[index] = 0;

    return 0;
}


uint32_t lwm2m_acl_permissions_reset(lwm2m_instance_t * p_instance,
                                     uint16_t           owner)
{
    NULL_PARAM_CHECK(p_instance);

    memset(p_instance->acl.access, 0, sizeof(uint16_t) * (1+LWM2M_MAX_SERVERS));
    memset(p_instance->acl.server, 0, sizeof(uint16_t) * (1+LWM2M_MAX_SERVERS));

    p_instance->acl.owner = owner;

    return 0;
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

    if (err_code != 0)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode the Instance ID
    err_code = lwm2m_tlv_integer_encode(p_buffer + index, p_buffer_len,
                                        LWM2M_ACL_INSTANCE_ID,
                                        p_instance->instance_id);

    if (err_code != 0)
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

    if (err_code != 0)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode owner.
    err_code = lwm2m_tlv_integer_encode(p_buffer + index, p_buffer_len,
                                        LWM2M_ACL_CONTROL_OWNER,
                                        p_instance->acl.owner);

    if (err_code != 0)
    {
        return err_code;
    }

    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    *p_buffer_len = index;

    return 0;
}

uint32_t lwm2m_acl_deserialize_tlv(uint8_t          * buffer,
                                   uint16_t           buffer_len,
                                   lwm2m_instance_t * p_instance)
{
    uint32_t    err_code;
    uint32_t    index = 0;
    lwm2m_tlv_t tlv;
    lwm2m_tlv_t acl_list = { 0 };
    uint16_t    object_id = 0;
    uint16_t    instance_id = 0;
    uint16_t    control_owner = 0;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, buffer, buffer_len);
        if (err_code != 0)
        {
            break;
        }

        switch (tlv.id)
        {
        case LWM2M_ACL_OBJECT_ID:
            lwm2m_tlv_bytebuffer_to_uint16(tlv.value, tlv.length, &object_id);
            break;
        case LWM2M_ACL_INSTANCE_ID:
            lwm2m_tlv_bytebuffer_to_uint16(tlv.value, tlv.length, &instance_id);
            break;
        case LWM2M_ACL_ACL:
            acl_list = tlv;
            break;
        case LWM2M_ACL_CONTROL_OWNER:
            lwm2m_tlv_bytebuffer_to_uint16(tlv.value, tlv.length, &control_owner);
            break;
        default:
            break;
        }
    }

    // Find the instance if none is provided
    if (p_instance == NULL)
    {
        err_code = lwm2m_lookup_instance(&p_instance, object_id, instance_id);
        if (err_code)
        {
            return err_code;
        }
    }

    if (object_id == p_instance->object_id &&
        instance_id == p_instance->instance_id)
    {
        if (control_owner != 0)
        {
            // Change owner
            p_instance->acl.owner = control_owner;
        }

        index = 0;
        while (index < acl_list.length)
        {
            err_code = lwm2m_tlv_decode(&tlv, &index, acl_list.value, acl_list.length);
            if (err_code != 0)
            {
                break;
            }

            // Add new permissions
            (void)lwm2m_acl_permissions_remove(p_instance, tlv.id);

            err_code = lwm2m_acl_permissions_add(p_instance, tlv.value[0], tlv.id);
            if (err_code != 0)
            {
                break;
            }
        }
    }
    else
    {
        return ENOENT;
    }

    return 0;
}
