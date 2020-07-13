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
#include <lwm2m_tlv.h>
#include <lwm2m_objects.h>
#include <lwm2m_remote.h>
#include <coap_message.h>

static lwm2m_alloc_t   m_alloc_fn = NULL;  /**< Memory allocator function, populated on @lwm2m_init. */
static lwm2m_free_t    m_free_fn = NULL;   /**< Memory free function, populated on @lwm2m_init. */

static bool m_access_control_enable_status = true;

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

uint32_t lwm2m_lookup_instance(lwm2m_instance_t  ** pp_instance,
                               uint16_t             object_id,
                               uint16_t             instance_id)
{
    if ((object_id == LWM2M_OBJ_ACCESS_CONTROL) && !m_access_control_enable_status)
    {
        return ENOENT;
    }

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

    /* In Access Control-disabled context, skip any Access Control instances. */
    if (((*p_instance)->object_id == LWM2M_OBJ_ACCESS_CONTROL) && !m_access_control_enable_status)
    {
        return lwm2m_instance_next(p_instance, prog);
    }

    return true;
}

uint32_t lwm2m_lookup_object(lwm2m_object_t  ** pp_object,
                             uint16_t           object_id)
{
    if ((object_id == LWM2M_OBJ_ACCESS_CONTROL) && !m_access_control_enable_status)
    {
        return ENOENT;
    }

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

void lwm2m_ctx_access_control_enable_status_set(bool enable_status)
{
    m_access_control_enable_status = enable_status;
}

bool lwm2m_ctx_access_control_enable_status_get(void)
{
    return m_access_control_enable_status;
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
    err_code = lwm2m_coap_handler_gen_attr_link(&object_id, 1, short_server_id, &p_buffer[buffer_index], &added_len, false);

    if (err_code != 0)
    {
        return err_code;
    }

    buffer_index += added_len;

    for (int i = 0; i < m_num_instances; ++i)
    {
        if (m_instances[i]->object_id == object_id)
        {
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
    err_code = lwm2m_coap_handler_gen_attr_link(path, 2, short_server_id, &p_buffer[buffer_index], &added_len, false);

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
            err_code = lwm2m_coap_handler_gen_attr_link(path, 3, short_server_id, &p_buffer[buffer_index], &added_len, false);

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
                LWM2M_MUTEX_UNLOCK();

                err_code = p_instance->callback(p_instance,
                                                LWM2M_INVALID_RESOURCE,
                                                operation,
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

            LWM2M_MUTEX_UNLOCK();

            err_code = p_instance->callback(p_instance,
                                            p_path[2],
                                            operation,
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

        if (short_server_id == LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID && curr_object == LWM2M_OBJ_ACCESS_CONTROL)
        {
            // Skip Access Control objects in Bootstrap Discover.
            continue;
        }

        if (curr_object == LWM2M_OBJ_ACCESS_CONTROL && !m_access_control_enable_status)
        {
            // Skip Access Control objects in Access Control-disabled context.
            continue;
        }

        bool instance_present = false;

        for (int j = 0; j < m_num_instances; ++j)
        {
            if (m_instances[j]->object_id == curr_object)
            {
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

    if (dry_run == true)
    {
        *p_buffer_len = dry_run_size;
    }
    else
    {
        *p_buffer_len = buffer_index;
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
