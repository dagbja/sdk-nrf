/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_remote.h>
#include <lwm2m_observer.h>
#include <lwm2m_objects_tlv.h>
#include <nrf_socket.h>

static lwm2m_observable_metadata_t       * m_observables[LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES];
static lwm2m_notif_attr_default_cb_t       notif_attr_default_set_cb;
static lwm2m_observable_reference_get_cb_t observable_reference_get_cb;
static lwm2m_uptime_get_cb_t               uptime_get_cb;
static lwm2m_request_remote_reconnect_cb_t request_remote_reconnect_cb;
static int64_t                             m_time_base;
static int64_t                             m_coap_con_interval = COAP_CON_NOTIFICATION_INTERVAL;
// lwm2m_notif_attribute_name and lwm2m_notif_attribute_type need to match the attributes
static const char * const lwm2m_notif_attribute_name[] = { "pmin", "pmax", "gt",
                                                        "lt", "st" };
static const uint8_t lwm2m_notif_attribute_type[] = { LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD,
                                                      LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD,
                                                      LWM2M_ATTRIBUTE_TYPE_GREATER_THAN,
                                                      LWM2M_ATTRIBUTE_TYPE_LESS_THAN,
                                                      LWM2M_ATTRIBUTE_TYPE_STEP };

static int observable_index_find(const void *p_observable, uint16_t ssid)
{
    if (!p_observable)
    {
        return -1;
    }

    for (int i = 0; i < ARRAY_SIZE(m_observables); i++)
    {
        if (!m_observables[i])
        {
            continue;
        }
        if ((m_observables[i]->observable == p_observable) && (m_observables[i]->ssid == ssid))
        {
            return i;
        }
    }

    return -1;
}

static int observable_empty_index_find(void)
{
    for (int i = 0; i < ARRAY_SIZE(m_observables); i++)
    {
        if (!m_observables[i])
        {
            return i;
        }
    }

    return -1;
}

static void lwm2m_observer_update_after_notification(int index)
{
    if (!(m_observables[index]->type & LWM2M_OBSERVABLE_TYPE_NO_CHECK))
    {
        m_observables[index]->prev_value = *(int32_t *)m_observables[index]->observable;
    }

    m_observables[index]->last_notification = 0;
    m_observables[index]->flags = 0;
}

void lwm2m_notif_attr_default_cb_set(lwm2m_notif_attr_default_cb_t callback)
{
    notif_attr_default_set_cb = callback;
}

void lwm2m_observable_reference_get_cb_set(lwm2m_observable_reference_get_cb_t callback)
{
    observable_reference_get_cb = callback;
}

void lwm2m_observable_uptime_cb_initialize(lwm2m_uptime_get_cb_t callback)
{
    uptime_get_cb = callback;
    m_time_base = uptime_get_cb();
}

void lwm2m_request_remote_reconnect_cb_set(lwm2m_request_remote_reconnect_cb_t callback)
{
    request_remote_reconnect_cb = callback;
}

int64_t lwm2m_coap_con_interval_get(void)
{
    return m_coap_con_interval;
}

void lwm2m_coap_con_interval_set(int64_t con_interval)
{
    m_coap_con_interval = con_interval;
}

static int lwm2m_notif_attribute_set(int index, uint8_t type, const void *p_value, int8_t assignment_level)
{
    lwm2m_notif_attribute_t *attr;

    if (index < 0 || !p_value || type >= LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE || type < 0)
    {
        return -EINVAL;
    }

    // Assert m_observables[index] != NULL
    attr = &m_observables[index]->attributes[type];

    /* Update current notification attribute according to the precedence rules (Section 5.1.1.) or unset if
       the uninitialized attribute assignment level is specified. */
    if (assignment_level >= attr->assignment_level || assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        attr->assignment_level = assignment_level;

        /* TODO: We currently don't represent any resources as floats, so store as integer regardless of type.
           If float was to be supported, then let attr->value.f = *(float *)p_value; for st, gt and lt. */
        switch (type)
        {
        case LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD:
        case LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD:
        case LWM2M_ATTRIBUTE_TYPE_GREATER_THAN:
        case LWM2M_ATTRIBUTE_TYPE_LESS_THAN:
        case LWM2M_ATTRIBUTE_TYPE_STEP:
            attr->value.i = *(int32_t *)p_value;
            break;
        default:
            // TODO: Assert for unsupported type.
            break;
        }
    }

    return 0;
}

static int lwm2m_observable_notif_attribute_init(int obs_index, uint8_t type)
{
    lwm2m_notif_attribute_t attribute;
    struct nrf_sockaddr *p_remote;
    uint8_t obs_type;
    int index = -1;
    const void *observable = NULL;

    // TODO: Assert m_observables[obs_index] != NULL

    if ((type == LWM2M_ATTRIBUTE_TYPE_GREATER_THAN) ||
        (type == LWM2M_ATTRIBUTE_TYPE_LESS_THAN) ||
        (type == LWM2M_ATTRIBUTE_TYPE_STEP))
    {
        // TODO: Figure out if Change Value Condition attributes can be also inherited.
        return -EINVAL;
    }

    // Find potential attributes set at a higher assignment level than the default.
    if (observable_reference_get_cb)
    {
        for (int level = m_observables[obs_index]->path_len - 1; level >= 0; level--)
        {
            observable = observable_reference_get_cb(m_observables[obs_index]->path, level, &obs_type);
            index = observable_index_find(observable, m_observables[obs_index]->ssid);

            if (index >= 0)
            {
                // Break at the attributes set at the highest assignment level.
                break;
            }
        }
    }

    if (index < 0)
    {
        if (!notif_attr_default_set_cb)
        {
            // Cannot set default attribute values.
            return -EIO;
        }

        // Finding remote should never fail at this stage.
        lwm2m_short_server_id_remote_find(&p_remote, m_observables[obs_index]->ssid);

        // Set default p_min or p_max attribute dictated by the server requesting the observation.
        notif_attr_default_set_cb(lwm2m_notif_attribute_type[type], (void *)&attribute.value.i, p_remote);
        attribute.assignment_level = LWM2M_ATTR_DEFAULT_ASSIGNMENT_LEVEL;

        // If default pmax value is 0, it must be ignored.
        if (type == LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD && attribute.value.i == 0)
        {
            attribute.assignment_level = LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL;
        }
    }
    else
    {
        /* If notification attributes set at a higher assignment level than the default have been found,
           copy to the attribute to be set. */
        attribute = m_observables[index]->attributes[type];
    }

    return lwm2m_notif_attribute_set(obs_index, type, (void *)&attribute.value.i, attribute.assignment_level);
}

int lwm2m_observable_metadata_init(struct nrf_sockaddr *p_remote, const uint16_t *p_path, uint8_t path_len)
{
    int index;
    uint32_t ret;
    const void *observable;
    uint8_t observable_type;
    uint16_t short_server_id;

    if (!p_path || !p_remote)
    {
        // TODO: Replace with assert.
        return -EINVAL;
    }

    ret = lwm2m_remote_short_server_id_find(&short_server_id, p_remote);
    if (ret != 0)
    {
        LWM2M_WRN("Failed to initialize observable metadata: unrecognized remote server.");
        return -ret;
    }

    if (!observable_reference_get_cb)
    {
        LWM2M_WRN("Failed to initialize observable metadata: no callback set to reference the observable");
        return -ENOENT;
    }

    observable = observable_reference_get_cb(p_path, path_len, &observable_type);
    if (!observable)
    {
        LWM2M_WRN("Failed to initialize observable metadata: structure is not observable");
        return -EINVAL;
    }

    index = observable_index_find(observable, short_server_id);
    if (index >= 0)
    {
        // Observable metadata structure already exists; reset the notification timers.
        lwm2m_observer_update_after_notification(index);
        m_observables[index]->con_notification = 0;
        return index;
    }

    index = observable_empty_index_find();
    if (index < 0)
    {
        // TODO: Replace with assert. Additional observable resources have been whitelisted, but the number of observers
        // has not been increased.
        LWM2M_WRN("Failed to initialize observable metadata: unsupported number of observers.");
        return -ENOMEM;
    }

    m_observables[index] = lwm2m_os_malloc(sizeof(lwm2m_observable_metadata_t));
    if (!m_observables[index])
    {
        LWM2M_WRN("Failed to initialize observable metadata: insufficient memory.");
        return -ENOMEM;
    }

    memset(m_observables[index], 0, sizeof(lwm2m_observable_metadata_t));

    m_observables[index]->path_len = path_len;
    for (int i = 0; i < path_len; i++)
    {
        m_observables[index]->path[i] = p_path[i];
    }

    for (int i = 0; i < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; i++)
    {
        m_observables[index]->attributes[i].assignment_level = LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL;
    }

    m_observables[index]->con_notification = uptime_get_cb();
    m_observables[index]->observable = observable;
    m_observables[index]->type = observable_type;
    m_observables[index]->ssid = short_server_id;

    lwm2m_observable_notif_attribute_init(index, LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD);
    lwm2m_observable_notif_attribute_init(index, LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD);

    return index;
}

static uint8_t pmin_attribute_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    lwm2m_notif_attribute_t attr = m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD];

    // Ignore uninitialized attributes.
    if (attr.assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        return 0;
    }

    if (m_observables[index]->last_notification >= attr.value.i)
    {
        return LWM2M_ATTRIBUTE_MIN_PERIOD_CODE;
    }

    return 0;
}

static uint8_t pmax_attribute_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    lwm2m_notif_attribute_t attr = m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD];

    // Ignore uninitialized attributes.
    if (attr.assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        return 0;
    }

    if (m_observables[index]->last_notification >= attr.value.i)
    {
        return LWM2M_ATTRIBUTE_MAX_PERIOD_CODE;
    }

    return 0;
}

static uint8_t gt_attribute_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    int32_t curr_value;
    lwm2m_notif_attribute_t attr = m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_GREATER_THAN];

    // Ignore uninitialized attributes.
    if (attr.assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        return 0;
    }

    // Check whether the threshold has been crossed.
    curr_value = *(int32_t *)m_observables[index]->observable;

    if (((curr_value > attr.value.i) && (m_observables[index]->prev_value < attr.value.i)) ||
        ((curr_value < attr.value.i) && (m_observables[index]->prev_value > attr.value.i)))
    {
        return LWM2M_ATTRIBUTE_GREATER_THAN_CODE;
    }

    return 0;
}

static uint8_t lt_attribute_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    int32_t curr_value;
    lwm2m_notif_attribute_t attr = m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_LESS_THAN];

    // Ignore uninitialized attributes.
    if (attr.assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        return 0;
    }

    // Check whether the threshold has been crossed.
    curr_value = *(int32_t *)m_observables[index]->observable;

    if (((curr_value > attr.value.i) && (m_observables[index]->prev_value < attr.value.i)) ||
        ((curr_value < attr.value.i) && (m_observables[index]->prev_value > attr.value.i)))
    {
        return LWM2M_ATTRIBUTE_LESS_THAN_CODE;
    }

    return 0;
}

static uint8_t st_attribute_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    int32_t curr_value;
    lwm2m_notif_attribute_t attr = m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_STEP];

    // Ignore uninitialized attributes.
    if (attr.assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
    {
        return 0;
    }

    curr_value = *(int32_t *)m_observables[index]->observable;

    if (abs(curr_value - m_observables[index]->prev_value) >= attr.value.i)
    {
        return LWM2M_ATTRIBUTE_STEP_CODE;
    }

    return 0;
}

static bool value_changed(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    int32_t curr_value = *(int32_t *)m_observables[index]->observable;

    if (curr_value != m_observables[index]->prev_value)
    {
        return true;
    }

    return false;
}

static void lwm2m_notif_attributes_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    m_observables[index]->flags |= pmin_attribute_check(index);
    m_observables[index]->flags |= pmax_attribute_check(index);

    // Objects, object instances, resource lists and strings do not support gt, lt or st attributes.
    if ((m_observables[index]->type & LWM2M_OBSERVABLE_TYPE_NO_CHECK) != 0)
    {
        return;
    }

    // Check Change Value Conditions only if the value has changed.
    if (value_changed(index))
    {
        m_observables[index]->flags |= gt_attribute_check(index);
        m_observables[index]->flags |= lt_attribute_check(index);
        m_observables[index]->flags |= st_attribute_check(index);
    }
}

static lwm2m_time_t lwm2m_observer_uptime_delta_get(void)
{
    int64_t delta, current_time;

    if (!uptime_get_cb)
    {
        return -1;
    }

    current_time = uptime_get_cb();
    delta = current_time - m_time_base;

    return (lwm2m_time_t)(delta / 1000);
}

static bool change_value_conditions_all_unset(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    if ((m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_GREATER_THAN].assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL) &&
        (m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_LESS_THAN].assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL) &&
        (m_observables[index]->attributes[LWM2M_ATTRIBUTE_TYPE_STEP].assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL))
    {
        return true;
    }

    return false;
}

static bool notification_send_check(int index)
{
    // Assert m_observables[index] != NULL && index >= 0.
    uint8_t flags = m_observables[index]->flags;

    if (flags & LWM2M_ATTRIBUTE_MAX_PERIOD_CODE)
    {
        return true;
    }

    /* Notifications regarding resources represented as strings or lists are currently
       sent only upon pmax. */
    if ((m_observables[index]->type & LWM2M_OBSERVABLE_TYPE_NO_CHECK) != 0)
    {
        return false;
    }

    if (!value_changed(index))
    {
        return false;
    }

    /* If the change value conditions are absent, send a notification when pmin
       has expired and the resource value has changed (valid for numerical resources only). */
    if (change_value_conditions_all_unset(index))
    {
        if (flags & LWM2M_ATTRIBUTE_MIN_PERIOD_CODE)
        {
            return true;
        }
    }
    else
    {
        if ((flags & LWM2M_ATTRIBUTE_MIN_PERIOD_CODE)   &&
            ((flags & LWM2M_ATTRIBUTE_GREATER_THAN_CODE) ||
             (flags & LWM2M_ATTRIBUTE_LESS_THAN_CODE) ||
             (flags & LWM2M_ATTRIBUTE_STEP_CODE)))
        {
            return true;
        }
    }

    return false;
}

bool lwm2m_observer_notification_is_con(const void *p_observable, uint16_t ssid)
{
    if (!p_observable)
    {
        return false;
    }

    int index = observable_index_find(p_observable, ssid);

    if (index < 0)
    {
        return false;
    }

    if ((m_observables[index]->con_notification + K_SECONDS(m_coap_con_interval)) < m_time_base)
    {
        m_observables[index]->con_notification = m_time_base;
        return true;
    }

    return false;
}

static void observer_notify_path(const uint16_t *p_path, uint8_t path_len, struct nrf_sockaddr *p_remote_server)
{
    uint8_t  payload[512];
    uint32_t payload_len = sizeof(payload);
    uint16_t short_server_id;
    uint32_t err_code;
    coap_observer_t * p_observer = NULL;
    const void * p_observable;

    p_observable = lwm2m_observable_reference_get(p_path, path_len);

    if (!p_observable)
    {
        LWM2M_WRN("Failed to notify the observer (%s): could not find the observable",
                  lwm2m_os_log_strdup(lwm2m_path_to_string(p_path, path_len)));
        return;
    }

    while (coap_observe_server_next_get(&p_observer, p_observer, (void *)p_observable) == 0)
    {
        lwm2m_remote_short_server_id_find(&short_server_id, p_observer->remote);

        if (lwm2m_remote_reconnecting_get(short_server_id))
        {
            /* Wait for reconnection */
            continue;
        }

        if (p_remote_server)
        {
            /* Only notify to given remote */
            if (memcmp(p_observer->remote, p_remote_server,
                       sizeof(struct nrf_sockaddr)) != 0) {
                continue;
            }
        }

        LWM2M_TRC("Observer found");
        err_code = lwm2m_tlv_element_encode(payload, &payload_len, p_path, path_len);

        if (err_code)
        {
            LWM2M_ERR("Failed to encode the observable (%s): %lu", lwm2m_os_log_strdup(lwm2m_path_to_string(p_path, path_len)), err_code);
            continue;
        }

        coap_msg_type_t type = (lwm2m_observer_notification_is_con(p_observable, short_server_id)) ? COAP_TYPE_CON : COAP_TYPE_NON;

        LWM2M_INF("Notify %s", lwm2m_os_log_strdup(lwm2m_path_to_string(p_path, path_len)));

        err_code = lwm2m_notify(payload, payload_len, p_observer, type);

        if (err_code)
        {
            LWM2M_INF("Failed to send the notification: %s (%ld)",
                      lwm2m_os_log_strdup(strerror(err_code)), err_code);

            if (request_remote_reconnect_cb)
            {
                request_remote_reconnect_cb(p_observer->remote);
            }
        }
    }
}

void lwm2m_observer_process(bool reconnect)
{
    struct nrf_sockaddr *remote;
    lwm2m_time_t delta = lwm2m_observer_uptime_delta_get();
    m_time_base = uptime_get_cb();

    if (delta < 0)
    {
        LWM2M_WRN("No callback set to retrieve the uptime");
        return;
    }

    for (int i = 0; i < LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES; i++)
    {
        if (!m_observables[i])
        {
            continue;
        }

        m_observables[i]->last_notification += delta;
        lwm2m_notif_attributes_check(i);

        if (notification_send_check(i))
        {
            // Finding remote should not fail at this stage.
            lwm2m_short_server_id_remote_find(&remote, m_observables[i]->ssid);
            observer_notify_path(m_observables[i]->path, m_observables[i]->path_len, remote);
            lwm2m_observer_update_after_notification(i);
        }
    }
}

int lwm2m_observable_notif_attributes_restore(const lwm2m_notif_attribute_t *p_attributes, const uint16_t *p_path, uint8_t path_len, uint16_t ssid)
{
    const void *observable;
    uint8_t observable_type;
    int ret, index;
    struct nrf_sockaddr *p_remote;

    if (!p_attributes || !p_path)
    {
        return -EINVAL;
    }

    if (!observable_reference_get_cb)
    {
        LWM2M_WRN("Failed to restore notification attributes: no callback set to reference the observable");
        return -EIO;
    }

    observable = observable_reference_get_cb(p_path, path_len, &observable_type);
    if (!observable)
    {
        return -ENOENT;
    }

    index = observable_index_find(observable, ssid);
    if (index < 0)
    {
        if (lwm2m_short_server_id_remote_find(&p_remote, ssid) != 0)
        {
            return -EINVAL;
        }

        index = lwm2m_observable_metadata_init(p_remote, p_path, path_len);
    }

    for (int i = 0; i < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; i++)
    {
        ret = lwm2m_notif_attribute_set(index, lwm2m_notif_attribute_type[i], &p_attributes[i].value.i, p_attributes[i].assignment_level);
        if (ret != 0)
        {
            return ret;
        }
    }

    return 0;
}

static bool lwm2m_notif_attribute_period_validate(const lwm2m_notif_attribute_t *p_pmin, const lwm2m_notif_attribute_t *p_pmax)
{
    if ((p_pmin->assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL) && (p_pmax->assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL))
    {
        if (p_pmax->value.i < p_pmin->value.i)
        {
            return false;
        }
    }

    return true;
}

static bool lwm2m_notif_attribute_change_value_validate(const lwm2m_notif_attribute_t *p_gt, const lwm2m_notif_attribute_t *p_lt, const lwm2m_notif_attribute_t *p_st)
{
    if ((p_lt->assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL) && (p_gt->assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL))
    {
        if (p_lt->value.i > p_gt->value.i)
        {
            return false;
        }

        if (p_st->assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
        {
            if ((p_lt->value.i + p_st->value.i * 2) > p_gt->value.i)
            {
                return false;
            }
        }
    }

    return true;
}

static int lwm2m_notif_attribute_period_update(lwm2m_notif_attribute_t **pp_attributes, int index, int8_t level)
{
    // TODO: Assert m_observables[index] != NULL && pp_attributes != NULL
    uint8_t attribute_type[] = { LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD,
                                 LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD };
    lwm2m_notif_attribute_t *attributes_new[ARRAY_SIZE(attribute_type)];
    int ret = 0;

    // Nothing to update.
    if (!pp_attributes[0] && !pp_attributes[1])
    {
        return ret;
    }

    for (int type = 0; type < ARRAY_SIZE(attribute_type); type++)
    {
        if (pp_attributes[type])
        {
            if (level >= m_observables[index]->attributes[attribute_type[type]].assignment_level)
            {
                attributes_new[type] = pp_attributes[type];
                continue;
            }
        }

        attributes_new[type] = &m_observables[index]->attributes[attribute_type[type]];
    }

    if (lwm2m_notif_attribute_period_validate(attributes_new[0], attributes_new[1]))
    {
        for (int type = 0; type < ARRAY_SIZE(attribute_type); type++)
        {
            if (!attributes_new[type])
            {
                continue;
            }

            ret = lwm2m_notif_attribute_set(index, attribute_type[type], &attributes_new[type]->value.i,
                                            attributes_new[type]->assignment_level);
            if (ret != 0)
            {
                return ret;
            }
        }
    }
    else
    {
        return -EINVAL;
    }

    return ret;
}

static int lwm2m_notif_attribute_change_value_update(lwm2m_notif_attribute_t **pp_attributes, int index, int8_t level)
{
    // TODO: Assert m_observables[index] != NULL && pp_attributes != NULL
    uint8_t attribute_type[] = { LWM2M_ATTRIBUTE_TYPE_GREATER_THAN,
                                 LWM2M_ATTRIBUTE_TYPE_LESS_THAN,
                                 LWM2M_ATTRIBUTE_TYPE_STEP };
    lwm2m_notif_attribute_t *attributes_new[ARRAY_SIZE(attribute_type)];
    int ret = 0;

    // Nothing to update.
    if (!pp_attributes[0] && !pp_attributes[1] && !pp_attributes[2])
    {
        return ret;
    }

    // The attributes gt, lt and st can only be assigned to resources or resource instances represented numerically.
    if (m_observables[index]->type & LWM2M_OBSERVABLE_TYPE_NO_CHECK)
    {
        return -EINVAL;
    }

    for (int type = 0; type < ARRAY_SIZE(attribute_type); type++)
    {
        if (pp_attributes[type])
        {
            if (level >= m_observables[index]->attributes[attribute_type[type]].assignment_level)
            {
                attributes_new[type] = pp_attributes[type];
                continue;
            }
        }

        attributes_new[type] = &m_observables[index]->attributes[attribute_type[type]];
    }

    if (lwm2m_notif_attribute_change_value_validate(attributes_new[0], attributes_new[1], attributes_new[2]))
    {
        for (int type = 0; type < ARRAY_SIZE(attribute_type); type++)
        {
            if (!attributes_new[type])
            {
                continue;
            }

            ret = lwm2m_notif_attribute_set(index, attribute_type[type], &attributes_new[type]->value.i,
                                            attributes_new[type]->assignment_level);
            if (ret != 0)
            {
                return ret;
            }
        }
    }
    else
    {
        return -EINVAL;
    }

    return ret;
}

static bool lwm2m_observable_is_init(int index)
{
    // Assert index >= 0 && !m_observables[index]
    if (m_observables[index]->path_len == 3)
    {
        const void *p_observable;
        uint16_t short_server_id, resource_id;
        uint8_t type;

        short_server_id = m_observables[index]->ssid;
        resource_id = m_observables[index]->path[2];
        p_observable = observable_reference_get_cb(m_observables[index]->path, m_observables[index]->path_len, &type);

        if (lwm2m_is_observed(short_server_id, p_observable))
        {
            return true;
        }
    }

    for (int i = 0; i < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; i++)
    {
        if (m_observables[index]->attributes[i].assignment_level == m_observables[index]->path_len)
        {
            return true;
        }
    }

    return false;
}

static void lwm2m_observable_metadata_free(int index)
{
    // Assert index >= 0
    if (m_observables[index])
    {
        lwm2m_os_free(m_observables[index]);
        m_observables[index] = NULL;
    }
}

void lwm2m_notif_attr_storage_update(const uint16_t *p_path, uint16_t path_len, struct nrf_sockaddr *p_remote)
{
    uint16_t short_server_id;
    uint32_t err_code;
    int index;
    const void *p_observable;
    uint8_t type;

    if (!p_path || !p_remote)
    {
        return;
    }

    err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_remote);
    if (err_code != 0)
    {
        return;
    }

    p_observable = observable_reference_get_cb(p_path, path_len, &type);
    if (!p_observable)
    {
        return;
    }

    index = observable_index_find(p_observable, short_server_id);
    if (index < 0)
    {
        return;
    }

    if (!lwm2m_observable_is_init(index))
    {
        /* Free the memory allocated for the observable and delete its corresponding
           entry in the non-volatile storage, if all of its attributes have been unset
           and it is not currently being observed. */
        lwm2m_notif_attr_storage_delete(m_observables[index]);
        lwm2m_observable_metadata_free(index);
    }
    else
    {
        // Update the observer entry in persistent storage, if it exists.
        lwm2m_notif_attr_storage_store(m_observables[index]);
    }
}

static int lwm2m_notif_attributes_update(int index, lwm2m_notif_attribute_t **pp_attributes, int8_t level)
{
    // pp_attributes MUST be of size LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE
    // Assert !pp_attributes && index >= 0
    lwm2m_notif_attribute_t *timing_condition[] = { pp_attributes[LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD],
                                                    pp_attributes[LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD] };
    lwm2m_notif_attribute_t *change_value_condition[] = { pp_attributes[LWM2M_ATTRIBUTE_TYPE_GREATER_THAN],
                                                          pp_attributes[LWM2M_ATTRIBUTE_TYPE_LESS_THAN],
                                                          pp_attributes[LWM2M_ATTRIBUTE_TYPE_STEP] };
    int ret;

    ret = lwm2m_notif_attribute_period_update(timing_condition, index, level);
    if (ret != 0)
    {
        return ret;
    }

    ret = lwm2m_notif_attribute_change_value_update(change_value_condition, index, level);
    if (ret != 0)
    {
        return ret;
    }

    return ret;
}

static void lwm2m_notif_attributes_normalize(void)
{
    // Start the post-processing at the lowest precedence level, as it might affect the observables at higher levels.
    for (int level = 1; level <= LWM2M_ATTR_RESOURCE_LEVEL; level++)
    {
        for (int j = 0; j < ARRAY_SIZE(m_observables); j++)
        {
            if (!m_observables[j])
            {
                continue;
            }

            if (m_observables[j]->path_len != level)
            {
                continue;
            }

            for (int type = 0; type < ARRAY_SIZE(lwm2m_notif_attribute_type); type++)
            {
                /* If any of the attribute have been unset, check whether there are values set at with lower procedence status or
                default values specified by the server. */
                if (m_observables[j]->attributes[lwm2m_notif_attribute_type[type]].assignment_level == LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
                {
                    int ret = lwm2m_observable_notif_attribute_init(j, lwm2m_notif_attribute_type[type]);
                    if (ret == 0)
                    {
                        // If the attribute has been modified, update the corresponding entry in NVS.
                        lwm2m_notif_attr_storage_store(m_observables[j]);
                    }
                }
            }
        }
    }
}

static bool is_query(const coap_message_t *p_request)
{
    for (int i = 0; i < p_request->options_count; i++)
    {
        if (p_request->options[i].number == COAP_OPT_URI_QUERY)
        {
            return true;
        }
    }

    return false;
}

int lwm2m_write_attribute_handler(const uint16_t *p_path, uint8_t path_len, const coap_message_t *p_request)
{
    NULL_PARAM_CHECK(p_path);
    NULL_PARAM_CHECK(p_request);

    lwm2m_notif_attribute_t *attributes_new[LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE];
    lwm2m_notif_attribute_t attributes_in[LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE];
    uint32_t value;
    char option[1024], *endptr;
    int ret = 0;
    uint16_t short_server_id;

    // Find the short server id of the observer.
    ret = lwm2m_remote_short_server_id_find(&short_server_id, p_request->remote);
    if (ret != 0)
    {
        return -ENOENT;
    }

    if (!is_query(p_request))
    {
        return -EINVAL;
    }

    // Parse the incoming write-attribute request and store the parameters.
    for (int i = 0; i < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; i++)
    {
        for (int j = 0; j < p_request->options_count; j++)
        {
            // The notification attributes are specified as CoAP Uri-Query option.
            if (p_request->options[j].number != COAP_OPT_URI_QUERY)
            {
                continue;
            }
            // Identify the notification attribute and retrieve its value.
            memcpy(option, p_request->options[j].data, p_request->options[j].length);
            option[p_request->options[j].length] = 0;

            if (strncmp(option, lwm2m_notif_attribute_name[i], strlen(lwm2m_notif_attribute_name[i])) == 0)
            {
                // If attribute present in the request, assign a pointer to it.
                attributes_new[i] = &attributes_in[i];
                // Check if the provided parameter value is empty; if so, the attribute is to be unset.
                if (p_request->options[j].length > strlen(lwm2m_notif_attribute_name[i]))
                {
                    attributes_in[i].assignment_level = path_len;
                }
                else
                {
                    attributes_in[i].assignment_level = LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL;
                }
                // p_min and p_max are specified as integers, while gt, lt and st are specified as floats.
                if (lwm2m_notif_attribute_type[i] == LWM2M_ATTRIBUTE_TYPE_MIN_PERIOD ||
                    lwm2m_notif_attribute_type[i] == LWM2M_ATTRIBUTE_TYPE_MAX_PERIOD)
                {
                    value = strtol(&option[strlen(lwm2m_notif_attribute_name[i]) + 1], &endptr, 10);
                }
                else
                {
                    // TODO: Remove the cast if it is decided to use float type.
                    value = (int)strtof(&option[strlen(lwm2m_notif_attribute_name[i]) + 1], &endptr);
                }
                attributes_in[i].value.i = value;
                break;
            }
            /* If an attribute is not present in the request, set the corresponding pointer to NULL to specify
               that it is not to be updated. */
            if (j == p_request->options_count - 1)
            {
                attributes_new[i] = NULL;
            }
        }
    }

    // Initialize the observable if it does not exist yet.
    lwm2m_observable_metadata_init(p_request->remote, p_path, path_len);

    /* Iterate the registered observables and match their corresponding URI to the one specified
       in the request. */
    for (int index = 0; index < LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES; index++)
    {
        if (!m_observables[index])
        {
            continue;
        }
        // Ignore the observables that do not correspond to the server that made the request.
        if (m_observables[index]->ssid != short_server_id)
        {
            continue;
        }

        for (int level = 0; level < path_len; level++)
        {
            if (m_observables[index]->path_len < path_len)
            {
                continue;
            }
            // Keep looping while the URIs are matching and until the assignment level is reached.
            if (m_observables[index]->path[level] == p_path[level])
            {
                if (level == path_len - 1)
                {
                    /* After reaching the assignment level, update the attributes of the observable
                       if the coherence check is successful and the precedence rules are respected. */
                    ret = lwm2m_notif_attributes_update(index, attributes_new, path_len);
                    if (ret != 0)
                    {
                        return ret;
                    }

                    lwm2m_notif_attr_storage_update(m_observables[index]->path, m_observables[index]->path_len, p_request->remote);
                }
                continue;
            }
            // Break the loop if the URIs are not matching up to the assignment level.
            break;
        }
    }

    lwm2m_notif_attributes_normalize();

    return ret;
}

const lwm2m_observable_metadata_t * const * lwm2m_observables_get(uint16_t *p_len)
{
    *p_len = LWM2M_MAX_OBSERVABLES_WITH_ATTRIBUTES;
    return (const lwm2m_observable_metadata_t * const * )m_observables;
}

uint32_t lwm2m_coap_handler_gen_attr_link(uint16_t const *p_path, uint16_t path_len, uint16_t short_server_id,
                uint8_t *p_buffer, uint32_t *p_buffer_len)
{
    int index;
    uint32_t buffer_index = 0;
    uint8_t  dry_run_buffer[128];
    uint32_t len = 0;
    uint8_t  type;
    const void *observable;

    if (!p_path || !p_buffer)
    {
        *p_buffer_len = 0;
        return EINVAL;
    }

    if (!observable_reference_get_cb)
    {
        *p_buffer_len = 0;
        return 0;
    }

    observable = observable_reference_get_cb(p_path, path_len, &type);
    if (!observable)
    {
        *p_buffer_len = 0;
        return 0;
    }

    index = observable_index_find(observable, short_server_id);
    if (index < 0)
    {
        *p_buffer_len = 0;
        return 0;
    }

    for (int i = 0; i < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; i++)
    {
        if (m_observables[index]->attributes[i].assignment_level >= path_len)
        {
            if (buffer_index >= sizeof(dry_run_buffer))
            {
                // Should never happen
                return ENOMEM;
            }

            len += snprintf((char *)&dry_run_buffer[buffer_index],
                            sizeof(dry_run_buffer) - len,
                            ";%s=%d", lwm2m_notif_attribute_name[i],
                            m_observables[index]->attributes[i].value.i);
            buffer_index = len;
        }
    }

    if (len <= *p_buffer_len)
    {
        memcpy(p_buffer, dry_run_buffer, len);
        *p_buffer_len = len;
    }
    else
    {
        *p_buffer_len = 0;
        return ENOMEM;
    }

    return 0;
}

const void * lwm2m_observable_reference_get(const uint16_t *p_path, uint8_t path_len)
{
    if (!p_path)
    {
        return NULL;
    }

    return observable_reference_get_cb(p_path, path_len, NULL);
}
