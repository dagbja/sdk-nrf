/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_remote.h>
#include <coap_api.h>

#define ENTRY_SIZE              sizeof(lwm2m_observer_storage_entry_t)
#define ENTRY_SIZE_EXCEPT_TOKEN (ENTRY_SIZE - (COAP_MESSAGE_TOKEN_MAX_LEN + sizeof(uint8_t)))

typedef struct __attribute__((__packed__))
{
    uint16_t            server_id;
    uint16_t            object_id;
    uint16_t            instance_id;
    uint16_t            resource_id;
    coap_content_type_t content_type;
    uint8_t             token_len;
    uint8_t             session_token[COAP_MESSAGE_TOKEN_MAX_LEN];
} lwm2m_observer_storage_entry_t;

static lwm2m_store_observer_cb_t store_callback;
static lwm2m_load_observer_cb_t  load_callback;
static lwm2m_del_observer_cb_t   del_callback;

static int lwm2m_observer_storage_lookup_storage_id(void * cur_entry)
{
    NULL_PARAM_CHECK(cur_entry);
    NULL_PARAM_CHECK(load_callback);

    uint8_t entry[ENTRY_SIZE];

    for (size_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        if (load_callback(i, (void *)&entry, ENTRY_SIZE) == 0)
        {
            if (memcmp(cur_entry, (void *)&entry, ENTRY_SIZE_EXCEPT_TOKEN) == 0)
            {
                return i;
            }
        }
    }

    return -ENOENT;
}

static int lwm2m_observer_storage_get_new_storage_id(void)
{
    NULL_PARAM_CHECK(load_callback);

    uint8_t entry;

    for (size_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        if (load_callback(i, &entry, 1) != 0)
        {
            return i;
        }
    }

    return -ENOMEM;
}

static int lwm2m_observer_storage_create_entry(coap_observer_t                * p_observer,
                                               lwm2m_observer_storage_entry_t * p_entry)
{
    NULL_PARAM_CHECK(p_observer);
    NULL_PARAM_CHECK(p_observer->p_userdata);
    NULL_PARAM_CHECK(p_observer->resource_of_interest);
    NULL_PARAM_CHECK(p_entry);

    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)p_observer->p_userdata;

    int err_code = lwm2m_remote_short_server_id_find(&p_entry->server_id, p_observer->remote);
    if (err_code != 0)
    {
        return err_code;
    }

    /* We are only interested in the 16-bit value that this pointer is pointing to
     * not the coap_resource_t object.
     */
    p_entry->resource_id  = *((uint16_t *)p_observer->resource_of_interest);
    p_entry->content_type = p_observer->ct;
    p_entry->token_len    = p_observer->token_len;

    p_entry->instance_id  = p_instance->instance_id;
    p_entry->object_id    = p_instance->object_id;

    memcpy(p_entry->session_token, p_observer->token, p_entry->token_len);

   return 0;
}

uint32_t lwm2m_observer_storage_set_callbacks(lwm2m_store_observer_cb_t store_cb,
                                              lwm2m_load_observer_cb_t load_cb,
                                              lwm2m_del_observer_cb_t del_cb)
{
    if (store_cb && load_cb && del_cb)
    {
        store_callback = store_cb;
        load_callback  = load_cb;
        del_callback   = del_cb;

        return 0;
    }

    return EINVAL;
}

uint32_t lwm2m_observer_storage_store(coap_observer_t  * p_observer)
{
    NULL_PARAM_CHECK(p_observer);
    NULL_PARAM_CHECK(p_observer->p_userdata);
    NULL_PARAM_CHECK(p_observer->remote);
    NULL_PARAM_CHECK(p_observer->resource_of_interest);
    NULL_PARAM_CHECK(store_callback);

    int                             sid;
    lwm2m_observer_storage_entry_t  entry;
    lwm2m_instance_t              * p_instance = (lwm2m_instance_t *)p_observer->p_userdata;
    uint16_t                        short_server_id;

    int err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_observer->remote);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_observer_storage_create_entry(p_observer, &entry);
    if (err_code != 0)
    {
        return err_code;
    }

    sid = lwm2m_observer_storage_lookup_storage_id(&entry);
    if (sid < 0)
    {
        sid = lwm2m_observer_storage_get_new_storage_id();

        if (sid < 0)
        {
            return -sid;
        }
    }
    else
    {
        LWM2M_INF("Observer /%d/%d/%d for server %d already exist in storage, updating entry",
                   p_instance->object_id,
                   p_instance->instance_id,
                   *((uint16_t *)p_observer->resource_of_interest),
                   short_server_id);
    }

    if (store_callback(sid, (void *)&entry, ENTRY_SIZE) != 0)
    {

        LWM2M_ERR("Failed to store observer /%d/%d/%d for server %d to storage",
                  p_instance->object_id,
                  p_instance->instance_id,
                  *((uint16_t *)p_observer->resource_of_interest),
                  short_server_id);

        return EIO;
    }

    return 0;

}

uint32_t lwm2m_observer_storage_delete(coap_observer_t  * p_observer)
{
    NULL_PARAM_CHECK(p_observer);
    NULL_PARAM_CHECK(p_observer->p_userdata);
    NULL_PARAM_CHECK(p_observer->resource_of_interest);
    NULL_PARAM_CHECK(p_observer->remote);
    NULL_PARAM_CHECK(del_callback);

    lwm2m_observer_storage_entry_t entry;
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)p_observer->p_userdata;
    uint16_t short_server_id;

    int err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_observer->remote);
    if (err_code != 0)
    {
        return err_code;
    }

    err_code = lwm2m_observer_storage_create_entry(p_observer, &entry);
    if (err_code != 0)
    {
        return err_code;
    }

    int sid = lwm2m_observer_storage_lookup_storage_id((void *)&entry);
    if (sid < 0)
    {
        return -sid;
    }

    err_code = del_callback(sid);
    if (err_code != 0) {

        LWM2M_ERR("Failed to delete observer /%d/%d/%d for server %d from storage failed:"
                  "%s (%ld), %s (%d)",
                  p_instance->object_id, p_instance->instance_id, *((uint16_t *)p_observer->resource_of_interest),
                  short_server_id, lwm2m_os_log_strdup(strerror(err_code)),
                  err_code, lwm2m_os_log_strdup(strerror(errno)), errno);


        return EIO;
    }

    return 0;
}

uint32_t lwm2m_observer_storage_restore(uint16_t                short_server_id,
                                        coap_transport_handle_t transport)
{
    NULL_PARAM_CHECK(load_callback);

    lwm2m_observer_storage_entry_t entry;
    uint32_t                       handle;
    uint32_t                       observer_count = 0;
    uint32_t                       rc             = 0;
    coap_observer_t                observer;

    for (uint32_t sid = 0; sid < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; sid++)
    {
        if (load_callback(sid, (void *)&entry, ENTRY_SIZE) != 0)
        {
            continue;
        }

        if (short_server_id == entry.server_id)
        {
            lwm2m_instance_t    * p_instance;
            struct nrf_sockaddr * p_remote;

            rc = lwm2m_lookup_instance(&p_instance, entry.object_id, entry.instance_id);
            if (rc != 0)
            {
                LWM2M_ERR("Locating observer /%d/%d failed: %s (%ld), %s (%d)",
                           entry.object_id, entry.instance_id,
                           lwm2m_os_log_strdup(strerror(rc)), rc,
                           lwm2m_os_log_strdup(strerror(errno)), errno);

                continue;
            }

            uint16_t * p_resource_ids = (uint16_t *)((uint8_t *)p_instance + p_instance->resource_ids_offset);

            int err_code = lwm2m_short_server_id_remote_find(&p_remote, entry.server_id);
            if (err_code != 0)
            {
                LWM2M_ERR("Finding remote for short server id: %d (observer: /%d/%d/%d) failed: %s (%ld), %s (%d)",
                           entry.server_id, entry.object_id, entry.instance_id, entry.resource_id,
                           lwm2m_os_log_strdup(strerror(rc)), rc,
                           lwm2m_os_log_strdup(strerror(errno)), errno);
            }

            observer.remote               = p_remote;
            observer.transport            = transport;
            observer.ct                   = entry.content_type;
            observer.p_userdata           = (void *)p_instance;
            observer.resource_of_interest = (coap_resource_t *)&p_resource_ids[entry.resource_id];
            observer.token_len            = entry.token_len;
            memcpy(observer.token, entry.session_token, entry.token_len);

            rc = coap_observe_server_register(&handle, &observer);

            if (rc != 0)
            {
                LWM2M_ERR("Loading observer with server ID: %d observing: /%d/%d/%d failed: %s (%ld), %s (%d)",
                           entry.server_id, entry.object_id, entry.instance_id, entry.resource_id,
                           lwm2m_os_log_strdup(strerror(rc)), rc,
                           lwm2m_os_log_strdup(strerror(errno)), errno);

                continue;
            }

            LWM2M_INF("Observer /%d/%d/%d restored, server: %d", entry.object_id,
                                                                 entry.instance_id,
                                                                 entry.resource_id,
                                                                 entry.server_id);

            observer_count++;
        }
    }

    return observer_count;
}

