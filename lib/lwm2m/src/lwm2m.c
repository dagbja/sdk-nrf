/*
 * Copyright (c) 2015 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_register.h>
#include <lwm2m_bootstrap.h>
#include <lwm2m_acl.h>
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>
#include <lwm2m_remote.h>
#include <coap_message.h>

static lwm2m_alloc_t   m_alloc_fn = NULL;  /**< Memory allocator function, populated on @lwm2m_init. */
static lwm2m_free_t    m_free_fn = NULL;   /**< Memory free function, populated on @lwm2m_init. */


void * lwm2m_malloc(size_t size)
{
    if (m_alloc_fn)
    {
        return m_alloc_fn(size);
    }
    else
    {
        return NULL;
    }
}


void lwm2m_free(void * p_memory)
{
    if (m_free_fn)
    {
        m_free_fn(p_memory);
    }
}


#if defined(CONFIG_NRF_LWM2M_ENABLE_LOGS)

static uint8_t op_desc_idx_lookup(uint8_t bitmask)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if ((bitmask >> i) == 0x1)
        {
            return i + 1;
        }
    }

    // If no bits where set in the bitmask.
    return 0;
}


static const char *m_operation_desc[] = {
    "NONE",
    "READ",
    "WRITE",
    "EXECUTE",
    "DELETE",
    "CREATE",
    "DISCOVER",
    "OBSERVE",
    "WRITE ATTR"
};

#endif // defined(CONFIG_NRF_LWM2M_ENABLE_LOGS)

/** Mutex used by LwM2M implementation. */
/* Mutex disabled for now.
struct k_mutex m_lwm2m_mutex;
*/

static lwm2m_object_t   * m_objects[LWM2M_COAP_HANDLER_MAX_OBJECTS];
static lwm2m_instance_t * m_instances[LWM2M_COAP_HANDLER_MAX_INSTANCES];
static uint16_t           m_num_objects;
static uint16_t           m_num_instances;


static bool coap_error_handler(uint32_t error_code, coap_message_t * p_message)
{
    // LWM2M_ERR("[CoAP]: Unhandled CoAP message received. Error code: %u", error_code);
    return lwm2m_coap_error_handler(error_code, p_message);
}


static void internal_coap_handler_init(void)
{
    memset(m_objects, 0, sizeof(m_objects));
    memset(m_instances, 0, sizeof(m_instances));

    m_num_objects   = 0;
    m_num_instances = 0;
}


static bool numbers_only(const char * p_str, uint16_t str_len)
{
    for (uint16_t i = 0; i < str_len; i++)
    {
        /* isdigit() implementation to avoid incompatibility between
         * the implementation provided by newlibc across compilers versions.
         */
        if (p_str[i] < '0' || p_str[i] > '9')
        {
            return false;
        }
    }
    return true;
}


static uint16_t internal_get_allowed_operations(lwm2m_instance_t * p_instance,
                                                uint16_t           short_server_id)
{
    uint16_t acl_access;
    uint32_t err_code;

    // Find access for the short_server_id.
    err_code = lwm2m_acl_permissions_check(&acl_access,
                                           p_instance,
                                           short_server_id);

    if (err_code != 0)
    {
        err_code = lwm2m_acl_permissions_check(&acl_access,
                                               p_instance,
                                               LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

        if (err_code != 0)
        {
             // Should not happen as p_instance should not be NULL
        }
    }


    if ((acl_access &  LWM2M_PERMISSION_READ) > 0)
    {
        // Observe and discover is allowed if READ is allowed.
        acl_access = (acl_access | LWM2M_OPERATION_CODE_DISCOVER | LWM2M_OPERATION_CODE_OBSERVE | LWM2M_OPERATION_CODE_WRITE_ATTR);
    }


    return acl_access;
}


uint32_t lwm2m_lookup_instance(lwm2m_instance_t  ** pp_instance,
                               uint16_t             object_id,
                               uint16_t             instance_id)
{
    for (int i = 0; i < m_num_instances; ++i)
    {
        if (m_instances[i]->object_id == object_id &&
            m_instances[i]->instance_id == instance_id)
        {
            if (m_instances[i]->callback == NULL)
            {
                return EINVAL;
            }

            *pp_instance = m_instances[i];

            return 0;
        }
    }

    return ENOENT;
}

bool lwm2m_instance_next(lwm2m_instance_t ** p_instance, size_t *prog)
{
    if (*p_instance == NULL) {
        *prog = 0;
    }

    if (*prog == m_num_instances) {
        return false;
    }

    __ASSERT(PART_OF_ARRAY(m_instances, m_instances + (*prog)),
         "Out of bounds");

    *p_instance = m_instances[(*prog)++];

    return true;
}

uint32_t lwm2m_lookup_object(lwm2m_object_t  ** pp_object,
                             uint16_t           object_id)
{
    for (int i = 0; i < m_num_objects; ++i)
    {
        if (m_objects[i]->object_id == object_id)
        {
            if (m_objects[i]->callback == NULL)
            {
                return EINVAL;
            }

            *pp_object = m_objects[i];

            return 0;
        }
    }

    return ENOENT;
}


static uint32_t op_code_resolve(lwm2m_instance_t * p_instance,
                                uint16_t           resource_id,
                                uint8_t          * operation)
{
    uint8_t  * operations   = (uint8_t *) p_instance + p_instance->operations_offset;
    uint16_t * resource_ids = (uint16_t *)((uint8_t *) p_instance + p_instance->resource_ids_offset);

    for (int i = 0; i < p_instance->num_resources; ++i)
    {
        if (resource_ids[i] == resource_id)
        {
            *operation = operations[i];

            return 0;
        }
    }

    return ENOENT;
}


static uint32_t internal_gen_acl_link(uint8_t * p_buffer, uint32_t * p_len, uint16_t short_server_id)
{
    uint32_t buffer_index    = 0;
    uint32_t buffer_max_size = *p_len;
    uint32_t buffer_len;

    uint8_t  dry_run_buffer[16]; // Maximum: "</65535/65535>,"
    bool     dry_run      = false;
    uint16_t dry_run_size = 0;

    if (p_buffer == NULL)
    {
        // Dry-run only, in order to calculate the size of the needed buffer.
        dry_run         = true;
        p_buffer        = dry_run_buffer;
        buffer_max_size = sizeof(dry_run_buffer);
    }

    for (int i = 0; i < m_num_instances; ++i)
    {
        if (m_instances[i]->object_id == LWM2M_OBJ_SECURITY)
        {
            // Skip ACL for Security objects.
            continue;
        }

        uint16_t allowed_ops = internal_get_allowed_operations(m_instances[i], short_server_id);
        if (allowed_ops == 0)
        {
            // No access.
            continue;
        }

        buffer_len = snprintf((char *)dry_run_buffer,
                              sizeof(dry_run_buffer),
                              ",</2/%u>",
                              m_instances[i]->acl.id);

        if (dry_run == true)
        {
            dry_run_size += buffer_len;
        }
        else if (buffer_index + buffer_len <= buffer_max_size)
        {
            memcpy(&p_buffer[buffer_index], dry_run_buffer, buffer_len);
            buffer_index += buffer_len;
        }
        else
        {
            return ENOMEM;
        }
    }

    if (dry_run == true)
    {
        *p_len = dry_run_size;
    }
    else
    {
        *p_len = buffer_index;
    }


    return 0;
}

static uint32_t internal_request_handle_acl(coap_message_t * p_request,
                                            uint16_t       * p_path,
                                            uint8_t          path_len,
                                            uint8_t          operation,
                                            uint16_t         short_server_id)
{
    uint32_t           err_code        = ENOENT;
    uint32_t           index           = 0;
    uint32_t           buffer_max_size = LWM2M_COAP_HANDLER_MAX_INSTANCES * LWM2M_ACL_TLV_SIZE;
    uint32_t           buffer_len      = buffer_max_size;
    bool               owner           = false;
    uint8_t            buffer[buffer_max_size];
    lwm2m_instance_t * current_instance = NULL;

    if (path_len == 1)
    {
        LWM2M_TRC(">> %s object /%u/ SSID: %u",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0],
                  short_server_id);


        switch(operation)
        {
            case LWM2M_OPERATION_CODE_READ:
            {
                uint32_t acl_buffer_len;
                uint8_t  acl_buffer[64];

                for(int i = 0; i < m_num_instances; ++i)
                {
                    if (m_instances[i]->object_id == LWM2M_OBJ_SECURITY)
                    {
                        // Skip ACL for Security objects.
                        continue;
                    }

                    uint16_t allowed_ops = internal_get_allowed_operations(m_instances[i], short_server_id);
                    if (allowed_ops == 0)
                    {
                        // No access.
                        continue;
                    }

                    acl_buffer_len = 64;
                    err_code = lwm2m_acl_serialize_tlv(acl_buffer, &acl_buffer_len, m_instances[i]);
                    if (err_code != 0)
                    {
                        // ENOMEM should not happen. Then it is a bug.
                        break;
                    }

                    lwm2m_tlv_t tlv = {
                        .id_type = TLV_TYPE_OBJECT,
                        .id      = m_instances[i]->acl.id,
                        .length  = acl_buffer_len
                    };
                    err_code = lwm2m_tlv_header_encode(buffer + index, &buffer_len, &tlv);

                    index += buffer_len;
                    buffer_len = buffer_max_size - index;

                    memcpy(buffer + index, acl_buffer, acl_buffer_len);

                    index += acl_buffer_len;
                    buffer_len = buffer_max_size - index;
                }

                err_code = lwm2m_respond_with_payload(buffer, index, COAP_CT_APP_LWM2M_TLV, p_request);
                break;
            }

            case LWM2M_OPERATION_CODE_DISCOVER:
            {
                uint32_t preamble_len = snprintf(buffer, sizeof(buffer), "</2>");
                buffer_len -= preamble_len;

                err_code = internal_gen_acl_link(&buffer[preamble_len], &buffer_len, short_server_id);
                if (err_code != 0)
                {
                    // This should not happen, it is a bug if the buffer is too small.
                    break;
                }

                err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);
                break;
            }

            case LWM2M_OPERATION_CODE_WRITE:
            case LWM2M_OPERATION_CODE_EXECUTE:
            case LWM2M_OPERATION_CODE_DELETE:
            case LWM2M_OPERATION_CODE_CREATE:
                break;

            default:
                break;
        }

        LWM2M_TRC("<< %s object /%u/",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0]);
    }

    if (path_len == 2)
    {
        LWM2M_TRC(">> %s instance /%u/%u/ SSID: %u",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0],
                  p_path[1],
                  short_server_id);

        // Check if short_server_id is owner.
        for(int i = 0; i < m_num_instances; ++i)
        {
            if (m_instances[i]->object_id == LWM2M_OBJ_SECURITY)
            {
                // Skip ACL for Security objects.
                continue;
            }

            if (m_instances[i]->acl.id == p_path[1])
            {
                current_instance = m_instances[i];
                if (m_instances[i]->acl.owner == short_server_id)
                {
                    owner = true;
                }
            }
        }

        if (current_instance == NULL)
        {
            return ENOENT;
        }

        switch (operation)
        {
            case LWM2M_OPERATION_CODE_READ:
            {
                bool found = false;
                for(int i = 0; i < m_num_instances; ++i)
                {
                    if (m_instances[i]->acl.id == p_path[1])
                    {
                        err_code = lwm2m_acl_serialize_tlv(buffer, &buffer_len, m_instances[i]);
                        if (err_code != 0)
                        {
                             // This should not happen, it is a bug if the buffer is too small.
                             break;
                        }

                        err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);
                        found = true;

                        break;
                    }
                }

                if (!found)
                {
                    err_code = ENOENT;
                }

                break;
            }

            case LWM2M_OPERATION_CODE_WRITE:
            {
                if ((short_server_id != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID) &&
                    (!owner))
                {
                    LWM2M_MUTEX_UNLOCK();

                    err_code = lwm2m_handler_error(short_server_id,
                                                   current_instance,
                                                   p_request,
                                                   EACCES);

                    LWM2M_MUTEX_LOCK();

                    if (err_code != 0)
                    {
                        err_code = lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
                        break;
                    }
                }


                err_code = lwm2m_acl_deserialize_tlv(p_request->payload, p_request->payload_len, current_instance);
                if (err_code != 0)
                {
                    LWM2M_MUTEX_UNLOCK();

                    err_code = lwm2m_handler_error(short_server_id,
                                                   current_instance,
                                                   p_request,
                                                   err_code);

                    LWM2M_MUTEX_LOCK();

                    if (err_code != 0)
                    {
                        err_code = lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
                    }
                }
                else
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
                }

                break;
            }

            case LWM2M_OPERATION_CODE_DISCOVER:
            {
                bool found = false;
                for(int i = 0; i < m_num_instances; ++i)
                {
                    if (m_instances[i]->acl.id == p_path[1])
                    {
                        // We always have the same resources.
                        buffer_len = snprintf((char *)buffer,
                                              buffer_max_size,
                                              "</2/%u>,</2/%u/0>,</2/%u/1>,</2/%u/2>,</2/%u/3>",
                                              m_instances[i]->acl.id,
                                              m_instances[i]->acl.id,
                                              m_instances[i]->acl.id,
                                              m_instances[i]->acl.id,
                                              m_instances[i]->acl.id);

                        err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    err_code = ENOENT;
                }
                break;
            }

            case LWM2M_OPERATION_CODE_EXECUTE:
            case LWM2M_OPERATION_CODE_DELETE:
            case LWM2M_OPERATION_CODE_CREATE:
            case LWM2M_OPERATION_CODE_OBSERVE:
                break;

            default:
                break;
        }

        LWM2M_TRC("<< %s instance /%u/%u/",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0],
                  p_path[1]);
    }

    if (path_len == 3)
    {
        LWM2M_TRC(">> %s instance /%u/%u/%u/ SSID: %u",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0],
                  p_path[1],
                  p_path[2],
                  short_server_id);

        // Check if owner
        for (int i = 0; i < m_num_instances; ++i)
        {
            if (m_instances[i]->object_id == LWM2M_OBJ_SECURITY)
            {
                // Skip ACL for Security objects.
                continue;
            }

            if (m_instances[i]->acl.id == p_path[1])
            {
                current_instance = m_instances[i];
            }
        }

        if (current_instance == NULL)
        {
            return ENOENT;
        }

        if (operation == LWM2M_OPERATION_CODE_DISCOVER)
        {
            buffer_len = snprintf((char *)buffer,
                                    buffer_max_size,
                                    "</2/%u/%u>",
                                    current_instance->acl.id,
                                    p_path[2]);

            err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LINK_FORMAT, p_request);

            return err_code;
        }

        switch (p_path[2])
        {
            /**
             * Writing a new object / instance id is not defined in our implementation.
             * That would be the same as moving ACL between instances. (which can be done, but seems unnecessary)
             */

            case LWM2M_ACL_OBJECT_ID:
            {
                if (operation == LWM2M_OPERATION_CODE_READ)
                {
                    err_code = lwm2m_tlv_integer_encode(buffer, &buffer_len,
                                                        LWM2M_ACL_OBJECT_ID,
                                                        current_instance->object_id);

                    if (err_code != 0)
                    {
                        return err_code;
                    }

                    err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);

                }

                break;
            }
            case LWM2M_ACL_INSTANCE_ID:
            {
                if (operation == LWM2M_OPERATION_CODE_READ)
                {
                    err_code = lwm2m_tlv_integer_encode(buffer, &buffer_len,
                                                        LWM2M_ACL_INSTANCE_ID,
                                                        current_instance->instance_id);

                    if (err_code != 0)
                    {
                        return err_code;
                    }

                    err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);

                }

                break;
            }
            case LWM2M_ACL_ACL:
            {
                if (operation == LWM2M_OPERATION_CODE_READ)
                {
                    // Encode the ACLs.
                    lwm2m_list_t list;
                    uint16_t list_identifiers[LWM2M_MAX_SERVERS];
                    uint16_t list_values[LWM2M_MAX_SERVERS];

                    list.type         = LWM2M_LIST_TYPE_UINT16;
                    list.p_id         = list_identifiers;
                    list.val.p_uint16 = list_values;
                    list.len          = 0;

                    for (int i = 1; i < (1+LWM2M_MAX_SERVERS); ++i)
                    {
                        if (current_instance->acl.server[i] != 0)
                        {
                            list_identifiers[list.len] = current_instance->acl.server[i];
                            list_values[list.len]      = current_instance->acl.access[i];
                            list.len++;
                        }
                    }

                    err_code = lwm2m_tlv_list_encode(buffer, &buffer_len, LWM2M_ACL_ACL, &list);

                    if (err_code != 0)
                    {
                        return err_code;
                    }

                    err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);
                }

                break;
            }
            case LWM2M_ACL_CONTROL_OWNER:
            {
                if (operation == LWM2M_OPERATION_CODE_READ)
                {
                    err_code = lwm2m_tlv_integer_encode(buffer, &buffer_len,
                                                       LWM2M_ACL_CONTROL_OWNER,
                                                       current_instance->acl.owner);

                    if (err_code != 0)
                    {
                        return err_code;
                    }

                    err_code = lwm2m_respond_with_payload(buffer, buffer_len, COAP_CT_APP_LWM2M_TLV, p_request);

                }

                break;
            }
            default:
                err_code = lwm2m_respond_with_code(COAP_CODE_501_NOT_IMPLEMENTED, p_request);
                break;

        }

        LWM2M_TRC("<< %s instance /%u/%u/%u/",
                  m_operation_desc[op_desc_idx_lookup(operation)],
                  p_path[0],
                  p_path[1],
                  p_path[2]);


    }

    return err_code;
}


uint32_t lwm2m_coap_handler_gen_object_link(uint16_t   object_id,
                                            uint16_t   short_server_id,
                                            uint8_t  * p_buffer,
                                            uint32_t * p_buffer_len)
{
    uint32_t err_code = 0;
    uint32_t buffer_index    = 0;
    uint32_t buffer_max_size = *p_buffer_len;
    uint32_t buffer_len, added_len;
    uint8_t  dry_run_buffer[16]; // Maximum: ",</65535/65535>"

    buffer_len = snprintf((char *)dry_run_buffer,
                          sizeof(dry_run_buffer),
                          "</%u>",
                          object_id);

    if (buffer_index + buffer_len <= buffer_max_size)
    {
        memcpy(&p_buffer[buffer_index], dry_run_buffer, buffer_len);
        buffer_index += buffer_len;
    }
    else
    {
        return ENOMEM;
    }

    added_len = buffer_max_size - buffer_index;
    err_code = lwm2m_coap_handler_gen_attr_link(&object_id, 1, short_server_id, &p_buffer[buffer_index], &added_len);

    if (err_code != 0)
    {
        return err_code;
    }

    buffer_index += added_len;

    for (int i = 0; i < m_num_instances; ++i)
    {
        if (m_instances[i]->object_id == object_id)
        {
            uint16_t allowed_ops = internal_get_allowed_operations(m_instances[i], short_server_id);
            if (allowed_ops == 0)
            {
                // No access.
                continue;
            }

            if (buffer_index + 1 <= buffer_max_size)
            {
                memcpy(&p_buffer[buffer_index], ",", 1);
                buffer_index += 1;
            }
            else
            {
                return ENOMEM;
            }

            added_len = buffer_max_size - buffer_index;
            err_code = lwm2m_coap_handler_gen_instance_link(m_instances[i], short_server_id, &p_buffer[buffer_index], &added_len);

            if (err_code != 0)
            {
                return err_code;
            }

            buffer_index += added_len;
        }
    }

    *p_buffer_len = buffer_index;

    return 0;
}


uint32_t lwm2m_coap_handler_gen_instance_link(lwm2m_instance_t * p_instance,
                                              uint16_t           short_server_id,
                                              uint8_t          * p_buffer,
                                              uint32_t         * p_buffer_len)
{
    uint32_t err_code;
    uint32_t buffer_index    = 0;
    uint32_t buffer_max_size = *p_buffer_len;
    uint32_t buffer_len;
    uint32_t added_len;
    uint16_t path[] = { p_instance->object_id, p_instance->instance_id, 0};
    uint8_t  dry_run_buffer[22]; // Maximum: ",</65535/65535/65535>"

    buffer_len = snprintf((char *)dry_run_buffer,
                          sizeof(dry_run_buffer),
                          "</%u/%u>",
                          p_instance->object_id,
                          p_instance->instance_id);

    if (buffer_index + buffer_len <= buffer_max_size)
    {
        memcpy(&p_buffer[buffer_index], dry_run_buffer, buffer_len);
        buffer_index += buffer_len;
    }
    else
    {
        return ENOMEM;
    }

    added_len = buffer_max_size - buffer_index;
    err_code = lwm2m_coap_handler_gen_attr_link(path, 2, short_server_id, &p_buffer[buffer_index], &added_len);

    if (err_code != 0)
    {
        return err_code;
    }

    buffer_index += added_len;

    uint16_t * p_resource_ids = (uint16_t *)((uint8_t *)p_instance + p_instance->resource_ids_offset);

    for (int i = 0; i < p_instance->num_resources; ++i)
    {
        uint8_t resource_operation = 0;
        err_code = op_code_resolve(p_instance, p_resource_ids[i], &resource_operation);

        if (err_code != 0)
        {
            // Op code for requested resource not found.
            continue;
        }

        if (resource_operation != 0)
        {
            buffer_len = snprintf((char *)dry_run_buffer,
                                  sizeof(dry_run_buffer),
                                  ",</%u/%u/%u>",
                                  p_instance->object_id,
                                  p_instance->instance_id,
                                  p_resource_ids[i]);

            if (buffer_index + buffer_len <= buffer_max_size)
            {
                memcpy(&p_buffer[buffer_index], dry_run_buffer, buffer_len);
                buffer_index += buffer_len;
            }
            else
            {
                return ENOMEM;
            }

            path[2] = p_resource_ids[i];
            added_len = buffer_max_size - buffer_index;
            err_code = lwm2m_coap_handler_gen_attr_link(path, 3, short_server_id, &p_buffer[buffer_index], &added_len);

            if (err_code != 0)
            {
                return err_code;
            }

            buffer_index += added_len;
        }
    }

    *p_buffer_len = buffer_index;

    return 0;
}


static uint32_t internal_request_handle(coap_message_t * p_request,
                                        uint16_t       * p_path,
                                        uint8_t          path_len,
                                        uint16_t         short_server_id)
{
    uint32_t err_code;
    uint8_t  operation    = LWM2M_OPERATION_CODE_NONE;
    uint32_t content_type = 0;

    err_code = coap_message_ct_mask_get(p_request, &content_type);
    if (err_code != 0)
    {
        return err_code;
    }

    switch (p_request->header.code)
    {
        case COAP_CODE_GET:
        {
            LWM2M_TRC("CoAP GET request");
            if (content_type == COAP_CT_MASK_APP_LINK_FORMAT) // Discover
            {
                operation = LWM2M_OPERATION_CODE_DISCOVER;
            }
            else if (coap_message_opt_present(p_request, COAP_OPT_OBSERVE) == 0) // Observe
            {
                operation = LWM2M_OPERATION_CODE_OBSERVE;
            }
            else // Read
            {
                operation = LWM2M_OPERATION_CODE_READ;
            }
            break;
        }

        case COAP_CODE_PUT:
        {
            if (content_type == COAP_CT_MASK_NONE)
            {
                operation = LWM2M_OPERATION_CODE_WRITE_ATTR;
            }
            else
            {
                operation = LWM2M_OPERATION_CODE_WRITE;
            }
            break;
        }

        case COAP_CODE_POST:
        {
            if (path_len == 1)
            {
                operation = LWM2M_OPERATION_CODE_CREATE;
            }
            else
            {
                operation = LWM2M_OPERATION_CODE_WRITE;
            }
            break;
        }

        case COAP_CODE_DELETE:
        {
            operation = LWM2M_OPERATION_CODE_DELETE;
            break;
        }

        default:
        {
            LWM2M_MUTEX_UNLOCK();

            err_code = lwm2m_handler_error(short_server_id,
                                           NULL,
                                           p_request,
                                           EINVAL);

            LWM2M_MUTEX_LOCK();

            if (err_code != 0)
            {
                err_code = lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
            }

            return err_code;
        }


    }

    /**
     * Jump out if the first path element is LWM2M_OBJ_ACL.
     *
     * This means that the object type is ACL, this is handled as an exception to avoid creating 2x instance_prototypes
     * per actual instance added.
     */
    if (path_len > 0 && p_path[0] == LWM2M_OBJ_ACL)
    {
        return internal_request_handle_acl(p_request, p_path, path_len, operation, short_server_id);
    }


    err_code = ENOENT;

    switch (path_len)
    {
        case 0:
        {

            if (operation == LWM2M_OPERATION_CODE_DELETE)
            {
                LWM2M_TRC(">> %s root /",
                          m_operation_desc[op_desc_idx_lookup(operation)]);

                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_coap_handler_root(LWM2M_OPERATION_CODE_DELETE, p_request);

                LWM2M_MUTEX_LOCK();

                LWM2M_TRC("<< %s root /",
                          m_operation_desc[op_desc_idx_lookup(operation)]);
            }
            else if (operation == LWM2M_OPERATION_CODE_DISCOVER)
            {
                if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
                {
                    // Bootstrap DISCOVER
                    err_code = lwm2m_respond_with_bs_discover_link(LWM2M_INVALID_INSTANCE, p_request);
                }
                else
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                }
            }
            else
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               EINVAL);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                }
                break;
            }
            break;
        }

        case 1:
        {
            LWM2M_TRC(">> %s object /%u/",
                      m_operation_desc[op_desc_idx_lookup(operation)],
                      p_path[0]);

            lwm2m_object_t * p_object;

            err_code = lwm2m_lookup_object(&p_object, p_path[0]);

            if (err_code != 0)
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               err_code);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                }
                break;
            }

            LWM2M_MUTEX_UNLOCK();

            err_code = p_object->callback(p_object, LWM2M_OBJECT_INSTANCE, operation, p_request);

            LWM2M_MUTEX_LOCK();

            LWM2M_TRC("<< %s object /%u/, result: %s",
                      m_operation_desc[op_desc_idx_lookup(operation)],
                      p_path[0],
                      (err_code == 0) ? "SUCCESS" : "NOT_FOUND");

            break;
        }

        case 2:
        {
            LWM2M_TRC(">> %s instance /%u/%u/",
                      m_operation_desc[op_desc_idx_lookup(operation)],
                      p_path[0],
                      p_path[1]);

            lwm2m_instance_t * p_instance;

            err_code = lwm2m_lookup_instance(&p_instance, p_path[0], p_path[1]);

            if (err_code == EINVAL)
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               EINVAL);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                }
                break;
            }



            if (err_code == 0)
            {

                // Lookup ACL.
                uint16_t allowed_ops = internal_get_allowed_operations(p_instance,
                                                                       short_server_id);

                LWM2M_MUTEX_UNLOCK();

                err_code = p_instance->callback(p_instance,
                                                LWM2M_INVALID_RESOURCE,
                                                (operation & allowed_ops),
                                                p_request);

                LWM2M_MUTEX_LOCK();

                LWM2M_TRC("<< %s instance /%u/%u/, result: %s",
                          m_operation_desc[op_desc_idx_lookup(operation)],
                          p_path[0],
                          p_path[1],
                          (err_code == 0) ? "SUCCESS" : "NOT_FOUND");
                break;
            }

            // Bootstrap can write to non-existing instances
            if (err_code == ENOENT &&
                operation == LWM2M_OPERATION_CODE_WRITE &&
                p_request->header.code == COAP_CODE_PUT)
            {
                LWM2M_TRC(">> %s object /%u/%u/",
                          m_operation_desc[op_desc_idx_lookup(operation)],
                          p_path[0],
                          p_path[1]);

                lwm2m_object_t * p_object;

                err_code = lwm2m_lookup_object(&p_object, p_path[0]);

                if (err_code != 0)
                {
                    LWM2M_MUTEX_UNLOCK();

                    err_code = lwm2m_handler_error(short_server_id,
                                                   NULL,
                                                   p_request,
                                                   err_code);

                    LWM2M_MUTEX_LOCK();

                    if (err_code != 0)
                    {
                        err_code = lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                    }
                    break;
                }

                LWM2M_MUTEX_UNLOCK();

                err_code = p_object->callback(p_object, p_path[1], operation, p_request);

                LWM2M_MUTEX_LOCK();

                LWM2M_TRC("<< %s object /%u/%u/, result: %s",
                          m_operation_desc[op_desc_idx_lookup(operation)],
                          p_path[0],
                          p_path[1],
                          (err_code == 0) ? "SUCCESS" : "NOT_FOUND");
            }

            // Instance was not found
            if (err_code == ENOENT)
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               ENOENT);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                }
                break;
            }

            break;
        }

        case 3:
        {
            if (operation == LWM2M_OPERATION_CODE_DELETE)
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               EINVAL);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                }
                break;
            }

            lwm2m_instance_t * p_instance;

            err_code = lwm2m_lookup_instance(&p_instance, p_path[0], p_path[1]);
            if (err_code != 0)
            {
                LWM2M_MUTEX_UNLOCK();

                err_code = lwm2m_handler_error(short_server_id,
                                               NULL,
                                               p_request,
                                               err_code);

                LWM2M_MUTEX_LOCK();

                if (err_code != 0)
                {
                    err_code = lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                }

                break;
            }

            if (p_request->header.code == COAP_CODE_POST)
            {
                // Check if it is WRITE or EXECUTE
                uint8_t resource_operation = 0;
                err_code = op_code_resolve(p_instance, p_path[2], &resource_operation);

                if (err_code != 0)
                {
                    // Op code for requested resource not found.
                    break;
                }

                if ((resource_operation & LWM2M_OPERATION_CODE_EXECUTE) > 0)
                {
                    operation = LWM2M_OPERATION_CODE_EXECUTE;
                }

                if ((resource_operation & LWM2M_OPERATION_CODE_WRITE) > 0)
                {
                    operation = LWM2M_OPERATION_CODE_WRITE;
                }
            }

            LWM2M_TRC(">> %s instance /%u/%u/%u/",
                        m_operation_desc[op_desc_idx_lookup(operation)],
                        p_path[0],
                        p_path[1],
                        p_path[2]);

            // Lookup ACL.
            uint16_t allowed_ops = internal_get_allowed_operations(p_instance,
                                                                   short_server_id);

            LWM2M_MUTEX_UNLOCK();

            err_code = p_instance->callback(p_instance,
                                            p_path[2],
                                            (operation & allowed_ops),
                                            p_request);

            LWM2M_MUTEX_LOCK();

            LWM2M_TRC("<< %s instance /%u/%u/%u/, result: %s",
                        m_operation_desc[op_desc_idx_lookup(operation)],
                        p_path[0],
                        p_path[1],
                        p_path[2],
                        (err_code == 0) ? "SUCCESS" : "NOT_FOUND");

            break;
        }

        default:
            break;
    }

    return err_code;
}


static uint32_t lwm2m_coap_handler_handle_request(coap_message_t * p_request)
{
    LWM2M_ENTRY();

    uint16_t index;
    uint16_t path[3];
    uint16_t short_server_id = LWM2M_ACL_DEFAULT_SHORT_SERVER_ID;
    char   * endptr;

    bool     is_numbers_only = true;
    uint16_t path_index      = 0;
    uint32_t err_code        = 0;

    LWM2M_MUTEX_LOCK();

    for (index = 0; index < p_request->options_count; index++)
    {
        if (p_request->options[index].number == COAP_OPT_URI_PATH)
        {
            uint16_t option_len = p_request->options[index].length;
            bool     numbers    = numbers_only((char *)p_request->options[index].data,
                                               option_len);
            if (numbers)
            {
                // Declare a temporary array that is 1 byte longer than the
                // option data in order to leave space for a terminating character.
                uint8_t option_data[option_len + 1];
                // Set the temporary array to zero.
                memset(option_data, 0, sizeof(option_data));
                // Copy the option data string to the temporary array.
                memcpy(option_data, p_request->options[index].data, option_len);

                // Convert the zero-terminated string to a long int value.
                path[path_index] = strtol((char *)option_data, &endptr, 10);

                ++path_index;

                if (endptr == ((char *)option_data))
                {
                    err_code = ENOENT;
                    break;
                }

                if (endptr != ((char *)option_data + option_len))
                {
                    err_code = ENOENT;
                    break;
                }
            }
            else
            {
                is_numbers_only = false;
                break;
            }
        }
    }

    if (err_code == 0)
    {
        err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);
        if (err_code == ENOENT)
        {
            // LWM2M remote not found. Setting it to be default short server id.
            short_server_id = LWM2M_ACL_DEFAULT_SHORT_SERVER_ID;
        }
        else if (err_code != 0)
        {
            // Should not happen. See lwm2m_remote_short_server_id_find return values.
            return err_code;
        }


        if (is_numbers_only == true)
        {
            err_code = internal_request_handle(p_request, path, path_index, short_server_id);
        }
        else
        {
            // If uri path did not consist of numbers only.
            char * requested_uri = NULL;

            for (index = 0; index < p_request->options_count; index++)
            {
                if (p_request->options[index].number == COAP_OPT_URI_PATH)
                {
                    requested_uri = (char *)p_request->options[index].data;

                    // Stop on first URI hit.
                    break;
                }
            }

            if (requested_uri == NULL)
            {
                err_code = ENOENT;
            }
            else
            {
                // Try to look up if there is a match with object with an alias name.
                for (int i = 0; i < m_num_objects; ++i)
                {
                    if (m_objects[i]->object_id == LWM2M_NAMED_OBJECT)
                    {
                        size_t size = strlen(m_objects[i]->p_alias_name);
                        if ((strncmp(m_objects[i]->p_alias_name, requested_uri, size) == 0))
                        {
                            if (m_objects[i]->callback == NULL)
                            {
                                err_code = EINVAL;
                                break;
                            }

                            LWM2M_MUTEX_UNLOCK();

                            err_code = m_objects[i]->callback(m_objects[i],
                                                              LWM2M_OBJECT_INSTANCE,
                                                              LWM2M_OPERATION_CODE_NONE,
                                                              p_request);

                            LWM2M_MUTEX_LOCK();

                            break;
                        }
                    }
                    else
                    {
                        // This is not a name object, return error code.
                        err_code = ENOENT;
                        break;
                    }
                }
            }
        }
    }

    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();

        err_code = lwm2m_handler_error(short_server_id,
                                       NULL,
                                       p_request,
                                       err_code);

        LWM2M_MUTEX_LOCK();

        if (err_code != 0)
        {
            err_code = lwm2m_respond_with_code(COAP_CODE_500_INTERNAL_SERVER_ERROR, p_request);
        }

        LWM2M_MUTEX_UNLOCK();
    }


    LWM2M_EXIT();

    return err_code;
}


uint32_t lwm2m_coap_handler_instance_add(lwm2m_instance_t * p_instance)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_instance);

    LWM2M_MUTEX_LOCK();

    if (m_num_instances == LWM2M_COAP_HANDLER_MAX_INSTANCES)
    {
        LWM2M_MUTEX_UNLOCK();
        LWM2M_WRN("Failed to register the instance /%d/%d, insufficient memory", p_instance->object_id, p_instance->instance_id);
        return ENOMEM;
    }

    m_instances[m_num_instances] = p_instance;
    ++m_num_instances;

    LWM2M_TRC("Adding /%d/%d", p_instance->object_id, p_instance->instance_id);

    LWM2M_MUTEX_UNLOCK();

    return 0;
}


uint32_t lwm2m_coap_handler_instance_delete(lwm2m_instance_t * p_instance)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_instance);

    LWM2M_MUTEX_LOCK();

    for (int i = 0; i < m_num_instances; ++i)
    {
        if ((m_instances[i]->object_id == p_instance->object_id) &&
            (m_instances[i]->instance_id == p_instance->instance_id))
        {
            // Move current last entry into this index position, and trim down the length.
            // If this is the last element, it cannot be accessed because the m_num_instances
            // count is 0.
            m_instances[i] = m_instances[m_num_instances - 1];
            --m_num_instances;

            LWM2M_MUTEX_UNLOCK();

            return 0;
        }
    }

    LWM2M_MUTEX_UNLOCK();

    return ENOENT;
}


uint32_t lwm2m_coap_handler_object_add(lwm2m_object_t * p_object)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_object);

    LWM2M_MUTEX_LOCK();

    if (m_num_objects == LWM2M_COAP_HANDLER_MAX_OBJECTS)
    {
        LWM2M_MUTEX_UNLOCK();

        return ENOMEM;
    }

    m_objects[m_num_objects] = p_object;
    ++m_num_objects;

    LWM2M_MUTEX_UNLOCK();

    return 0;
}


uint32_t lwm2m_coap_handler_object_delete(lwm2m_object_t * p_object)
{
    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_object);

    LWM2M_MUTEX_LOCK();

    for (int i = 0; i < m_num_objects; ++i)
    {
        if ( m_objects[i]->object_id == p_object->object_id)
        {
            // Move current last entry into this index position, and trim down the length.
            // If this is the last element, it cannot be accessed because the m_num_objects
            // count is 0.
            m_objects[i] = m_objects[m_num_objects - 1];
            --m_num_objects;

            LWM2M_MUTEX_UNLOCK();

            return 0;
        }
    }

    LWM2M_MUTEX_UNLOCK();

    return ENOENT;
}


uint32_t lwm2m_coap_handler_gen_link_format(uint16_t object_id, uint16_t short_server_id, uint8_t * p_buffer, uint16_t * p_buffer_len)
{

    LWM2M_ENTRY();

    NULL_PARAM_CHECK(p_buffer_len);

    LWM2M_MUTEX_LOCK();

    uint16_t  buffer_index = 0;
    uint16_t  buffer_len;
    uint8_t * p_string_buffer;
    uint16_t  buffer_max_size;
    uint32_t  acl_link_size = 0;
    uint32_t  err_code = 0;
    bool      first_entry = true;

    uint8_t  dry_run_buffer[16]; // Maximum: "</65535/65535>,"
    bool     dry_run      = false;
    uint16_t dry_run_size = 0;

    if (p_buffer == NULL)
    {
        // Dry-run only, in order to calculate the size of the needed buffer.
        dry_run         = true;
        p_string_buffer = dry_run_buffer;
        buffer_max_size = sizeof(dry_run_buffer);
    }
    else
    {
        p_string_buffer = p_buffer;
        buffer_max_size = *p_buffer_len;
    }

    if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
    {
        // Bootstrap DISCOVER
        buffer_len = snprintf((char *)dry_run_buffer,
                              sizeof(dry_run_buffer),
                              "lwm2m=\"1.0\"");
        if (dry_run == true)
        {
            dry_run_size += buffer_len;
        }
        else if (buffer_index + buffer_len <= buffer_max_size)
        {
            memcpy(&p_string_buffer[buffer_index], dry_run_buffer, buffer_len);
            buffer_index += buffer_len;
        }
        else
        {
            LWM2M_MUTEX_UNLOCK();

            return ENOMEM;
        }

        first_entry = false;
    }

    for (int i = 0; i < m_num_objects; ++i)
    {
        uint16_t curr_object = m_objects[i]->object_id;

        if (curr_object == LWM2M_NAMED_OBJECT)
        {
            // Skip this object as it is a named object.
            continue;
        }

        if (object_id != LWM2M_INVALID_INSTANCE && object_id != curr_object)
        {
            // Not interested in this object.
            continue;
        }

        if (short_server_id != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID && curr_object == LWM2M_OBJ_SECURITY)
        {
            // Skip Security objects.
            continue;
        }

        bool instance_present = false;

        for (int j = 0; j < m_num_instances; ++j)
        {
            if (m_instances[j]->object_id == curr_object)
            {
                if (short_server_id != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
                {
                    uint16_t allowed_ops = internal_get_allowed_operations(m_instances[j], short_server_id);
                    if (allowed_ops == 0)
                    {
                        // No access.
                        continue;
                    }
                }

                instance_present = true;

                buffer_len = snprintf((char *)dry_run_buffer,
                                      sizeof(dry_run_buffer),
                                      "%s</%u/%u>",
                                      (first_entry ? "" : ","),
                                      m_instances[j]->object_id,
                                      m_instances[j]->instance_id);
                if (dry_run == true)
                {
                    dry_run_size += buffer_len;
                }
                else if (buffer_index + buffer_len <= buffer_max_size)
                {
                    memcpy(&p_string_buffer[buffer_index], dry_run_buffer, buffer_len);
                    buffer_index += buffer_len;
                }
                else
                {
                    LWM2M_MUTEX_UNLOCK();

                    return ENOMEM;
                }

                if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
                {
                    // Bootstrap DISCOVER
                    uint16_t ssid = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID;
                    if (m_instances[j]->object_id == LWM2M_OBJ_SECURITY)
                    {
                        ssid = ((lwm2m_security_t *)m_instances[j])->short_server_id;
                    }
                    else if (m_instances[j]->object_id == LWM2M_OBJ_SERVER)
                    {
                        ssid = ((lwm2m_server_t *)m_instances[j])->short_server_id;
                    }

                    if (ssid != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
                    {
                        buffer_len = snprintf((char *)dry_run_buffer,
                                            sizeof(dry_run_buffer),
                                            ";ssid=%u",
                                            ssid);
                        if (dry_run == true)
                        {
                            dry_run_size += buffer_len;
                        }
                        else if (buffer_index + buffer_len <= buffer_max_size)
                        {
                            memcpy(&p_string_buffer[buffer_index], dry_run_buffer, buffer_len);
                            buffer_index += buffer_len;
                        }
                        else
                        {
                            LWM2M_MUTEX_UNLOCK();

                            return ENOMEM;
                        }
                    }
                }

                first_entry = false;
            }
        }

        if (!instance_present)
        {
            buffer_len = snprintf((char *)dry_run_buffer,
                                  sizeof(dry_run_buffer),
                                  "%s</%u>",
                                  (first_entry ? "" : ","),
                                  curr_object);
            if (dry_run == true)
            {
                dry_run_size += buffer_len;
            }
            else if (buffer_index + buffer_len <= buffer_max_size)
            {
                memcpy(&p_string_buffer[buffer_index], dry_run_buffer, buffer_len);
                buffer_index += buffer_len;
            }
            else
            {
                LWM2M_MUTEX_UNLOCK();

                return ENOMEM;
            }

            first_entry = false;
        }
    }

    // Do not add ACL for Bootstrap DISCOVER
    if (short_server_id != LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID)
    {
        // Write ACL object
        if (dry_run == true)
        {
            acl_link_size = 0;
            err_code = internal_gen_acl_link(NULL, &acl_link_size, short_server_id);
        }
        else
        {
            acl_link_size = buffer_max_size - buffer_index;
            err_code = internal_gen_acl_link(&p_string_buffer[buffer_index], &acl_link_size, short_server_id);
        }
    }

    if (dry_run == true)
    {
        *p_buffer_len = dry_run_size + acl_link_size;
    }
    else
    {
        *p_buffer_len = buffer_index + acl_link_size;
    }

    LWM2M_MUTEX_UNLOCK();

    return err_code;
}


int32_t lwm2m_list_integer_get(lwm2m_list_t * p_list, uint32_t idx)
{
    if (!p_list || idx > p_list->len)
    {
        return 0;
    }

    int32_t value = 0;

    switch (p_list->type)
    {
        case LWM2M_LIST_TYPE_UINT8:
            value = p_list->val.p_uint8[idx];
            break;

        case LWM2M_LIST_TYPE_UINT16:
            value = p_list->val.p_uint16[idx];
            break;

        case LWM2M_LIST_TYPE_INT32:
            value = p_list->val.p_int32[idx];
            break;

        default:
            break;
    }

    return value;
}


uint32_t lwm2m_list_integer_set(lwm2m_list_t * p_list, uint32_t idx, int32_t value)
{
    if (!p_list || idx > p_list->len || idx >= p_list->max_len)
    {
        return EMSGSIZE;
    }

    switch (p_list->type)
    {
        case LWM2M_LIST_TYPE_UINT8:
            p_list->val.p_uint8[idx] = (uint8_t) value;
            break;

        case LWM2M_LIST_TYPE_UINT16:
            p_list->val.p_uint16[idx] = (uint16_t) value;
            break;

        case LWM2M_LIST_TYPE_INT32:
            p_list->val.p_int32[idx] = value;
            break;

        default:
            return EINVAL;
    }

    if (idx == p_list->len) {
        // Added a value to the list
        p_list->len = idx + 1;
    }

    return 0;
}


uint32_t lwm2m_list_integer_append(lwm2m_list_t * p_list, int32_t value)
{
    return lwm2m_list_integer_set(p_list, p_list->len, value);
}


lwm2m_string_t * lwm2m_list_string_get(lwm2m_list_t * p_list, uint32_t idx)
{
    if (!p_list || !p_list->val.p_string || idx >= p_list->len)
    {
        return NULL;
    }

    switch (p_list->type)
    {
        case LWM2M_LIST_TYPE_STRING:
            return &p_list->val.p_string[idx];
        default:
            return NULL;
    }
}


uint32_t lwm2m_list_string_set(lwm2m_list_t * p_list, uint32_t idx, const uint8_t * p_value, uint16_t value_len)
{
    uint32_t err_code;

    if (!p_list || idx > p_list->len || idx >= p_list->max_len)
    {
        return EMSGSIZE;
    }

    switch (p_list->type)
    {
        case LWM2M_LIST_TYPE_STRING:
            err_code = lwm2m_bytebuffer_to_string(p_value, value_len, &p_list->val.p_string[idx]);
            break;

        default:
            return EINVAL;
    }

    if (idx == p_list->len) {
        // Added a value to the list
        p_list->len = idx + 1;
    }

    return err_code;
}


uint32_t lwm2m_list_string_append(lwm2m_list_t * p_list, uint8_t * p_value, uint16_t value_len)
{
    return lwm2m_list_string_set(p_list, p_list->len, p_value, value_len);
}


uint32_t lwm2m_init(lwm2m_alloc_t alloc_fn, lwm2m_free_t free_fn)
{
    if ((alloc_fn == NULL) || (free_fn == NULL))
    {
        return EINVAL;
    }

/* Disable mutex for now.
    k_mutex_init(&m_lwm2m_mutex);
*/

    LWM2M_MUTEX_LOCK();

    uint32_t err_code;

    m_alloc_fn = alloc_fn;
    m_free_fn  = free_fn;

    err_code = internal_lwm2m_register_init();
    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }

    err_code = internal_lwm2m_bootstrap_init();
    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }

    err_code = coap_error_handler_register(coap_error_handler);
    if (err_code != 0)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }

    internal_coap_handler_init();

    err_code = coap_request_handler_register(lwm2m_coap_handler_handle_request);

    LWM2M_MUTEX_UNLOCK();

    return err_code;
}

const char * lwm2m_path_to_string(const uint16_t *p_path, uint8_t path_len)
{
    static char uri_path[sizeof("/65535/65535/65535/65535")]; /* Longest URI possible */
    uint32_t index = 0;

    if (!p_path)
    {
        return "";
    }

    for (int i = 0; i < path_len; i++)
    {
        index += snprintf(&uri_path[index], sizeof(uri_path) - index, "/%u", p_path[i]);
    }

    uri_path[index] = '\0';

    return uri_path;
}

uint32_t lwm2m_update_acl_ssid(uint16_t old_ssid, uint16_t new_ssid)
{
    for (int i = 0; i < m_num_instances; ++i)
    {
        if (m_instances[i]->acl.owner == old_ssid)
        {
            m_instances[i]->acl.owner = new_ssid;
        }

        for (int j = 1; j < (1+LWM2M_MAX_SERVERS); ++j)
        {
            if (m_instances[i]->acl.server[j] == old_ssid)
            {
                m_instances[i]->acl.server[j] = new_ssid;
            }
        }
    }

    return 0;
}