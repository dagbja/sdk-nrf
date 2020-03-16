/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_observer.h>
#include <lwm2m_remote.h>
#include <coap_api.h>
#include <coap_observe_api.h>

#define OBSERVER_ENTRY_SIZE              sizeof(lwm2m_observer_storage_entry_t)
#define OBSERVER_ENTRY_SIZE_EXCEPT_TOKEN (OBSERVER_ENTRY_SIZE - (COAP_MESSAGE_TOKEN_MAX_LEN + sizeof(uint8_t)))

#define NOTIF_ATTR_ENTRY_SIZE            sizeof(lwm2m_notif_attr_storage_entry_t)
#define NOTIF_ATTR_ENTRY_SIZE_CONSTANT   (NOTIF_ATTR_ENTRY_SIZE - (LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE * sizeof(lwm2m_notif_attribute_t)))

typedef struct __attribute__((__packed__))
{
    uint16_t            short_server_id;
    uint16_t            object_id;
    uint16_t            instance_id;
    uint16_t            resource_id;
    coap_content_type_t content_type;
    uint8_t             token_len;
    uint8_t             session_token[COAP_MESSAGE_TOKEN_MAX_LEN];
} lwm2m_observer_storage_entry_t;

typedef struct __attribute__((__packed__))
{
    uint16_t                short_server_id;
    uint16_t                path[LWM2M_URI_PATH_MAX_LEN];
    uint8_t                 path_len;
    lwm2m_notif_attribute_t attributes[LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE];
} lwm2m_notif_attr_storage_entry_t;

static lwm2m_store_observer_cb_t observer_store_cb;
static lwm2m_load_observer_cb_t  observer_load_cb;
static lwm2m_del_observer_cb_t   observer_delete_cb;

static lwm2m_store_notif_attr_cb_t notif_attr_store_cb;
static lwm2m_load_notif_attr_cb_t  notif_attr_load_cb;
static lwm2m_del_notif_attr_cb_t   notif_attr_delete_cb;

static int lwm2m_observer_storage_lookup_storage_id(void * cur_entry)
{
    NULL_PARAM_CHECK(cur_entry);
    NULL_PARAM_CHECK(observer_load_cb);

    uint8_t entry[OBSERVER_ENTRY_SIZE];

    for (size_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        if (observer_load_cb(i, (void *)&entry, OBSERVER_ENTRY_SIZE) == 0)
        {
            if (memcmp(cur_entry, (void *)&entry, OBSERVER_ENTRY_SIZE_EXCEPT_TOKEN) == 0)
            {
                return i;
            }
        }
    }

    return -ENOENT;
}

static int lwm2m_notif_attr_storage_lookup_storage_id(const void * cur_entry)
{
    NULL_PARAM_CHECK(cur_entry);
    NULL_PARAM_CHECK(notif_attr_load_cb);

    uint8_t entry[NOTIF_ATTR_ENTRY_SIZE];

    for (size_t i = 0; i < LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES; i++)
    {
        if (notif_attr_load_cb(i, (void *)&entry, NOTIF_ATTR_ENTRY_SIZE) == 0)
        {
            if (memcmp(cur_entry, (void *)&entry, NOTIF_ATTR_ENTRY_SIZE_CONSTANT) == 0)
            {
                return i;
            }
        }
    }

    return -ENOENT;
}

static int lwm2m_observer_storage_get_new_storage_id(void)
{
    NULL_PARAM_CHECK(observer_load_cb);

    uint8_t entry;

    for (size_t i = 0; i < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; i++)
    {
        if (observer_load_cb(i, &entry, 1) != 0)
        {
            return i;
        }
    }

    return -ENOMEM;
}

static int lwm2m_notif_attr_storage_get_new_storage_id(void)
{
    NULL_PARAM_CHECK(notif_attr_load_cb);

    uint8_t entry;

    for (size_t i = 0; i < LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES; i++)
    {
        if (notif_attr_load_cb(i, &entry, 1) != 0)
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

    int err_code = lwm2m_remote_short_server_id_find(&p_entry->short_server_id, p_observer->remote);
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

static int lwm2m_notif_attr_storage_create_entry(const lwm2m_observable_metadata_t      * p_metadata,
                                                 lwm2m_notif_attr_storage_entry_t       * p_entry)
{
    NULL_PARAM_CHECK(p_metadata);
    NULL_PARAM_CHECK(p_metadata->observable);
    NULL_PARAM_CHECK(p_entry);

    memcpy(p_entry->attributes, p_metadata->attributes, sizeof(lwm2m_notif_attribute_t) * LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE);
    memcpy(p_entry->path, p_metadata->path, LWM2M_URI_PATH_MAX_LEN * sizeof(uint16_t));
    p_entry->path_len = p_metadata->path_len;
    p_entry->short_server_id = p_metadata->ssid;

   return 0;
}

uint32_t lwm2m_observer_storage_set_callbacks(lwm2m_store_observer_cb_t store_cb,
                                              lwm2m_load_observer_cb_t load_cb,
                                              lwm2m_del_observer_cb_t del_cb)
{
    if (store_cb && load_cb && del_cb)
    {
        observer_store_cb = store_cb;
        observer_load_cb  = load_cb;
        observer_delete_cb   = del_cb;

        return 0;
    }

    return EINVAL;
}

uint32_t lwm2m_notif_attr_storage_set_callbacks(lwm2m_store_notif_attr_cb_t store_cb,
                                                lwm2m_load_notif_attr_cb_t load_cb,
                                                lwm2m_del_notif_attr_cb_t del_cb)
{
    if (store_cb && load_cb && del_cb)
    {
        notif_attr_store_cb = store_cb;
        notif_attr_load_cb = load_cb;
        notif_attr_delete_cb = del_cb;

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
    NULL_PARAM_CHECK(observer_store_cb);

    int                             sid;
    lwm2m_observer_storage_entry_t  entry;
    lwm2m_instance_t              * p_instance = (lwm2m_instance_t *)p_observer->p_userdata;
    uint16_t                        short_server_id;

    /* Silence warnings */
    (void) p_instance;

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

    if (observer_store_cb(sid, (void *)&entry, OBSERVER_ENTRY_SIZE) != 0)
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

uint32_t lwm2m_notif_attr_storage_store(const lwm2m_observable_metadata_t * p_metadata)
{
    NULL_PARAM_CHECK(p_metadata);
    NULL_PARAM_CHECK(p_metadata->observable);
    NULL_PARAM_CHECK(notif_attr_store_cb);

    lwm2m_notif_attr_storage_entry_t  entry;
    int err_code, sid;

    err_code = lwm2m_notif_attr_storage_create_entry(p_metadata, &entry);
    if (err_code != 0)
    {
        return err_code;
    }

    sid = lwm2m_notif_attr_storage_lookup_storage_id(&entry);
    if (sid < 0)
    {
        sid = lwm2m_notif_attr_storage_get_new_storage_id();

        if (sid < 0)
        {
            return -sid;
        }
    }

    if (notif_attr_store_cb(sid, (void *)&entry, NOTIF_ATTR_ENTRY_SIZE) != 0)
    {

        LWM2M_ERR("Failed to store notification attributes in the non-volatile storage");
        return EIO;
    }
    entry = (lwm2m_notif_attr_storage_entry_t)entry;

    return 0;
}

uint32_t lwm2m_observer_storage_delete(coap_observer_t  * p_observer)
{
    NULL_PARAM_CHECK(p_observer);
    NULL_PARAM_CHECK(p_observer->p_userdata);
    NULL_PARAM_CHECK(p_observer->resource_of_interest);
    NULL_PARAM_CHECK(p_observer->remote);
    NULL_PARAM_CHECK(observer_delete_cb);

    lwm2m_observer_storage_entry_t entry;
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)p_observer->p_userdata;
    uint16_t short_server_id;

    /* Silence warnings */
    (void) p_instance;

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

    err_code = observer_delete_cb(sid);
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

uint32_t lwm2m_notif_attr_storage_delete(const lwm2m_observable_metadata_t * p_metadata)
{
    NULL_PARAM_CHECK(p_metadata);
    NULL_PARAM_CHECK(p_metadata->observable);
    NULL_PARAM_CHECK(notif_attr_delete_cb);

    lwm2m_notif_attr_storage_entry_t entry;
    int err_code, sid;

    err_code = lwm2m_notif_attr_storage_create_entry(p_metadata, &entry);
    if (err_code != 0)
    {
        return err_code;
    }

    sid = lwm2m_notif_attr_storage_lookup_storage_id((void *)&entry);
    if (sid < 0)
    {
        return -sid;
    }

    err_code = notif_attr_delete_cb(sid);
    if (err_code != 0) {
        LWM2M_ERR("Failed to delete notification attributes from the non-volatile storage");
        return EIO;
    }

    return 0;
}

void lwm2m_notif_attr_storage_delete_all(void)
{
    uint32_t err_code;
    uint16_t len;
    lwm2m_observable_metadata_t **observables = (lwm2m_observable_metadata_t **)lwm2m_observables_get(&len);

    if (!observables)
    {
        return;
    }

    for (int i = 0; i < len; i++)
    {
        if (!observables[i])
        {
            continue;
        }

        err_code = lwm2m_notif_attr_storage_delete(observables[i]);
        if (err_code == EIO)
        {
            return;
        }
    }
}

uint32_t lwm2m_observer_storage_restore(uint16_t                short_server_id,
                                        coap_transport_handle_t transport)
{
    NULL_PARAM_CHECK(observer_load_cb);

    lwm2m_observer_storage_entry_t entry;
    uint32_t                       handle;
    uint32_t                       observer_count = 0;
    uint32_t                       rc             = 0;
    coap_observer_t                observer;

    for (uint32_t sid = 0; sid < CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS; sid++)
    {
        if (observer_load_cb(sid, (void *)&entry, OBSERVER_ENTRY_SIZE) != 0)
        {
            continue;
        }

        if (short_server_id == entry.short_server_id)
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

            int err_code = lwm2m_short_server_id_remote_find(&p_remote, entry.short_server_id);
            if (err_code != 0)
            {
                LWM2M_ERR("Finding remote for short server id: %d (observer: /%d/%d/%d) failed: %s (%ld), %s (%d)",
                           entry.short_server_id, entry.object_id, entry.instance_id, entry.resource_id,
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
                           entry.short_server_id, entry.object_id, entry.instance_id, entry.resource_id,
                           lwm2m_os_log_strdup(strerror(rc)), rc,
                           lwm2m_os_log_strdup(strerror(errno)), errno);

                continue;
            }

            LWM2M_INF("Observer /%d/%d/%d restored, server: %d", entry.object_id,
                                                                 entry.instance_id,
                                                                 entry.resource_id,
                                                                 entry.short_server_id);

            uint16_t path[] = { entry.object_id, entry.instance_id, entry.resource_id };

            lwm2m_observable_metadata_init(p_remote, path, ARRAY_SIZE(path));

            observer_count++;
        }
    }

    return observer_count;
}

void lwm2m_observer_storage_delete_all(void)
{
    coap_observer_t * p_observer = NULL;

    while (coap_observe_server_next_get(&p_observer, p_observer, NULL) == 0)
    {
        lwm2m_observer_storage_delete(p_observer);
    }
}

uint32_t lwm2m_notif_attr_storage_restore(uint16_t short_server_id)
{
    NULL_PARAM_CHECK(notif_attr_load_cb);

    lwm2m_notif_attr_storage_entry_t entry;
    int err_code;

    for (uint32_t sid = 0; sid < LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES; sid++)
    {
        if (notif_attr_load_cb(sid, (void *)&entry, NOTIF_ATTR_ENTRY_SIZE) != 0)
        {
            continue;
        }

        if (entry.short_server_id != short_server_id)
        {
            continue;
        }

        err_code = lwm2m_observable_notif_attributes_restore(entry.attributes, entry.path, entry.path_len, entry.short_server_id);
        if (err_code != 0)
        {
            LWM2M_ERR("Loading notification attributes failed: %s (%ld), %s (%d)",
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(strerror(errno)), errno);
            continue;
        }
    }

    return 0;
}
