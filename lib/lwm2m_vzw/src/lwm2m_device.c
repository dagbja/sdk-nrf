/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_device.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <lwm2m_os.h>
#include <coap_message.h>
#include <common.h>
#include <at_interface.h>
#include <lwm2m_vzw_main.h>
#include <dfusock.h>
#include <nrf_socket.h>
#include <lwm2m_carrier.h>
#include <coap_option.h>
#include <coap_observe_api.h>
#include <app_debug.h>

#define VERIZON_RESOURCE 30000

#define MAX_TIMEZONE_LEN 64

#define TIMEZONE_MIN_OFFSET -720
#define TIMEZONE_MAX_OFFSET  840

static lwm2m_object_t m_object_device;    /**< Device base object. */
static lwm2m_device_t m_instance_device;  /**< Device object instance. */
static lwm2m_string_t m_verizon_resources[2];

static int64_t m_con_time_start[sizeof(((lwm2m_device_t *)0)->resource_ids)];

void lwm2m_device_notify_resource(uint16_t resource_id); // Forward declare.

static uint32_t tlv_device_verizon_encode(uint16_t instance_id, uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    ARG_UNUSED(instance_id);

    lwm2m_list_t list =
    {
        .type         = LWM2M_LIST_TYPE_STRING,
        .val.p_string = m_verizon_resources,
        .len          = 2,
        .max_len      = ARRAY_SIZE(m_verizon_resources)
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}

static uint32_t tlv_device_resource_decode(uint16_t instance_id, lwm2m_tlv_t * p_tlv)
{
    return 0;
}

static void lwm2m_device_current_time_update(void)
{
    m_instance_device.current_time = lwm2m_carrier_utc_time_read();
}

static void lwm2m_device_utc_offset_update(void)
{
    char string_buffer[17];

    int utc_offset = lwm2m_carrier_utc_offset_read();
    snprintf(string_buffer, sizeof(string_buffer), "UTC%+03d:%02d", utc_offset / 60, abs(utc_offset % 60));

    (void)lwm2m_bytebuffer_to_string(string_buffer, strlen(string_buffer), &m_instance_device.utc_offset);
}

static void lwm2m_device_timezone_update(void)
{
    const char *p_tz = lwm2m_carrier_timezone_read();
    int tz_len = strlen(p_tz);

    if (tz_len > MAX_TIMEZONE_LEN)
    {
        tz_len = MAX_TIMEZONE_LEN;
    }

    (void)lwm2m_bytebuffer_to_string((char*)p_tz, tz_len, &m_instance_device.timezone);
}

static void lwm2m_device_timezone_write(lwm2m_device_t * p_device)
{
    char string_buffer[MAX_TIMEZONE_LEN+1];
    int len;

    if (p_device->timezone.len <= MAX_TIMEZONE_LEN)
    {
        len = p_device->timezone.len;
    }
    else
    {
        len = MAX_TIMEZONE_LEN;
    }

    strncpy(string_buffer, p_device->timezone.p_val, len);
    string_buffer[len] = '\0'; // null-terminated

    lwm2m_carrier_timezone_write(string_buffer);
}

static int lwm2m_device_utc_offset_write(lwm2m_device_t * p_device)
{
    char string_buffer[10];
    int utc_offset_mins = 0;

    if (p_device->utc_offset.len < 10)
    {
        strncpy(string_buffer, p_device->utc_offset.p_val, p_device->utc_offset.len);
        string_buffer[p_device->utc_offset.len] = '\0';

        int offset = 0;
        int len = p_device->utc_offset.len;

        // Detect UTC notation
        if (strncmp(string_buffer, "UTC", 3) == 0)
        {
            offset = 3;
            len -= 3;
        }

        if (len <= 3)
        {
            // +hh
            char *p_tail;
            utc_offset_mins = (int32_t)strtol(&string_buffer[offset], &p_tail, 10) * 60;
            if (p_tail == string_buffer)
            {
                return -EINVAL;
            }
        }
        else if (len == 5 || len == 6)
        {
            // +hhmm or +hh:mm

            if (len == 6 && string_buffer[offset + 3] != ':')
            {
                return -EINVAL;
            }

            char *p_tail;
            int mins_offset = offset + len - 2;
            int tmp_mins = (int32_t)strtol(&string_buffer[mins_offset], &p_tail, 10);
            if (p_tail == &string_buffer[mins_offset])
            {
                return -EINVAL;
            }
            string_buffer[mins_offset] = '\0';

            utc_offset_mins = (int32_t)strtol(&string_buffer[offset], &p_tail, 10) * 60;
            if (p_tail == &string_buffer[offset])
            {
                return -EINVAL;
            }

            if (utc_offset_mins < 0)
            {
                utc_offset_mins -= tmp_mins;
            }
            else
            {
                utc_offset_mins += tmp_mins;
            }

            if (utc_offset_mins < TIMEZONE_MIN_OFFSET || utc_offset_mins > TIMEZONE_MAX_OFFSET)
            {
                return -EINVAL;
            }
        }
        else
        {
            return -EINVAL;
        }
    }
    else
    {
        return -EINVAL;
    }

    lwm2m_carrier_utc_offset_write(utc_offset_mins);
    return 0;
}


int lwm2m_device_set_sim_iccid(char *p_iccid, uint32_t iccid_len)
{
    if (p_iccid == NULL) {
        return EINVAL;
    }

    return lwm2m_bytebuffer_to_string(p_iccid, iccid_len, &m_verizon_resources[0]);
}


char * lwm2m_device_get_sim_iccid(uint32_t * iccid_len)
{
    if (iccid_len == NULL) {
        return NULL;
    }

    *iccid_len = m_verizon_resources[0].len;
    return m_verizon_resources[0].p_val;
}


/**@brief Callback function for device instances. */
uint32_t device_instance_callback(lwm2m_instance_t * p_instance,
                                  uint16_t           resource_id,
                                  uint8_t            op_code,
                                  coap_message_t   * p_request)
{
    LWM2M_TRC("device_instance_callback");

    uint16_t access = 0;
    uint32_t err_code = common_lwm2m_access_remote_get(&access,
                                                       p_instance,
                                                       p_request->remote);
    if (err_code != 0)
    {
        return err_code;
    }

    // Set op_code to 0 if access not allowed for that op_code.
    // op_code has the same bit pattern as ACL operates with.
    op_code = (access & op_code);

    if (op_code == 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    uint16_t instance_id = p_instance->instance_id;

    if (instance_id != 0)
    {
        (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    uint8_t  buffer[300];
    uint32_t buffer_size = sizeof(buffer);

    if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        uint32_t observe_option = 0;
        for (uint8_t index = 0; index < p_request->options_count; index++)
        {
            if (p_request->options[index].number == COAP_OPT_OBSERVE)
            {
                err_code = coap_opt_uint_decode(&observe_option,
                                                p_request->options[index].length,
                                                p_request->options[index].data);
                break;
            }
        }

        if (err_code == 0)
        {
            if (observe_option == 0) // Observe start
            {
                // Whitelist the resources that support observe.
                switch (resource_id)
                {
                    // case LWM2M_DEVICE_MANUFACTURER:
                    // case LWM2M_DEVICE_MODEL_NUMBER:
                    // case LWM2M_DEVICE_SERIAL_NUMBER:
                    // case LWM2M_DEVICE_FIRMWARE_VERSION:
                    case LWM2M_DEVICE_AVAILABLE_POWER_SOURCES:
                    case LWM2M_DEVICE_POWER_SOURCE_VOLTAGE:
                    case LWM2M_DEVICE_POWER_SOURCE_CURRENT:
                    case LWM2M_DEVICE_BATTERY_LEVEL:
                    // case LWM2M_DEVICE_MEMORY_FREE:
                    case LWM2M_DEVICE_ERROR_CODE:
                    // case LWM2M_DEVICE_CURRENT_TIME:
                    // case LWM2M_DEVICE_UTC_OFFSET:
                    // case LWM2M_DEVICE_TIMEZONE:
                    // case LWM2M_DEVICE_SUPPORTED_BINDINGS:
                    case LWM2M_DEVICE_DEVICE_TYPE:
                    case LWM2M_DEVICE_HARDWARE_VERSION:
                    case LWM2M_DEVICE_SOFTWARE_VERSION:
                    case LWM2M_DEVICE_BATTERY_STATUS:
                    case LWM2M_DEVICE_MEMORY_TOTAL:
                    {
                        LWM2M_INF("Observe requested on resource /3/%i/%i", p_instance->instance_id, resource_id);
                        err_code = lwm2m_tlv_device_encode(buffer,
                                                        &buffer_size,
                                                        resource_id,
                                                        &m_instance_device);

                        err_code = lwm2m_observe_register(buffer,
                                                        buffer_size,
                                                        m_instance_device.proto.expire_time,
                                                        p_request,
                                                        COAP_CT_APP_LWM2M_TLV,
                                                        resource_id,
                                                        p_instance);

                        m_con_time_start[resource_id] = lwm2m_os_uptime_get();
                        break;
                    }

                    case LWM2M_INVALID_RESOURCE: // By design LWM2M_INVALID_RESOURCE indicates that this is on instance level.
                    {
                        // Process the GET request as usual.
                        LWM2M_INF("Observe requested on instance /3/%i, no slots", p_instance->instance_id);
                        op_code = LWM2M_OPERATION_CODE_READ;
                        break;
                    }

                    default:
                    {
                        // Process the GET request as usual.
                        LWM2M_INF("Observe requested on resource /3/%i/%i, no slots", p_instance->instance_id, resource_id);
                        op_code = LWM2M_OPERATION_CODE_READ;
                        break;
                    }
                }
            }
            else if (observe_option == 1) // Observe stop
            {
                if (resource_id == LWM2M_INVALID_RESOURCE) {
                    LWM2M_INF("Observe cancel on instance /3/%i, no  match", p_instance->instance_id);
                } else {
                    LWM2M_INF("Observe cancel on resource /3/%i/%i", p_instance->instance_id, resource_id);
                    lwm2m_observe_unregister(p_request->remote, (void *)&m_instance_device.resource_ids[resource_id]);
                }

                // Process the GET request as usual.
                op_code = LWM2M_OPERATION_CODE_READ;
            }
            else
            {
                (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                return 0;
            }
        }
    }


    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_device_verizon_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
            // Update requested resource
            switch (resource_id)
            {
                case LWM2M_DEVICE_CURRENT_TIME:
                    lwm2m_device_current_time_update();
                    break;
                case LWM2M_DEVICE_UTC_OFFSET:
                    lwm2m_device_utc_offset_update();
                    break;
                case LWM2M_DEVICE_TIMEZONE:
                    lwm2m_device_timezone_update();
                    break;
                case LWM2M_NAMED_OBJECT:
                    lwm2m_device_current_time_update();
                    lwm2m_device_utc_offset_update();
                    lwm2m_device_timezone_update();
                    break;
                default:
                    break;
            }

            err_code = lwm2m_tlv_device_encode(buffer,
                                               &buffer_size,
                                               resource_id,
                                               &m_instance_device);

            if (err_code == ENOENT)
            {
                (void)lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
                return 0;
            }

            if (resource_id == LWM2M_NAMED_OBJECT)
            {
                uint32_t added_size = sizeof(buffer) - buffer_size;
                err_code = tlv_device_verizon_encode(instance_id, buffer + buffer_size, &added_size);
                buffer_size += added_size;
            }
        }

        if (err_code != 0)
        {
            return err_code;
        }

        (void)lwm2m_respond_with_payload(buffer, buffer_size, COAP_CT_APP_LWM2M_TLV, p_request);
    }
    else if (op_code == LWM2M_OPERATION_CODE_WRITE)
    {
        uint32_t mask = 0;
        err_code = coap_message_ct_mask_get(p_request, &mask);

        if (err_code != 0)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
            return 0;
        }

        if (mask & COAP_CT_MASK_APP_LWM2M_TLV)
        {
            err_code = lwm2m_tlv_device_decode(&m_instance_device,
                                               p_request->payload,
                                               p_request->payload_len,
                                               tlv_device_resource_decode);
        }
        else if ((mask & COAP_CT_MASK_PLAIN_TEXT) || (mask & COAP_CT_MASK_APP_OCTET_STREAM))
        {
            err_code = lwm2m_plain_text_device_decode(&m_instance_device,
                                                      resource_id,
                                                      p_request->payload,
                                                      p_request->payload_len);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_request);
            return 0;
        }

        if (err_code == 0)
        {
            int err;

            switch (resource_id)
            {
                case LWM2M_DEVICE_CURRENT_TIME:
                    lwm2m_carrier_utc_time_write(m_instance_device.current_time);
                    break;
                case LWM2M_DEVICE_UTC_OFFSET:
                    err = lwm2m_device_utc_offset_write(&m_instance_device);
                    if (err != 0)
                    {
                        (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                        return 0;
                    }
                    break;
                case LWM2M_DEVICE_TIMEZONE:
                    lwm2m_device_timezone_write(&m_instance_device);
                    break;
                case LWM2M_NAMED_OBJECT:
                {
                    lwm2m_carrier_utc_time_write(m_instance_device.current_time);
                    err = lwm2m_device_utc_offset_write(&m_instance_device);
                    if (err != 0)
                    {
                        (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
                        return 0;
                    }
                    lwm2m_device_timezone_write(&m_instance_device);
                    break;
                }
                default:
                    (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                    return 0;
            }
            (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
        }
        else if (err_code == ENOTSUP)
        {
            (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        }
        else
        {
            (void)lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_request);
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_EXECUTE)
    {
        switch (resource_id)
        {
            case LWM2M_DEVICE_FACTORY_RESET:
            {
                lwm2m_factory_reset();
            }
            /* FALLTHROUGH */

            case LWM2M_DEVICE_REBOOT:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                // FIXME: This sleep is needed to ensure the response is sent before closing the socket.
                lwm2m_os_sleep(1000);

                lwm2m_request_reset();
                break;
            }

            case LWM2M_DEVICE_RESET_ERROR_CODE:
            {
                m_instance_device.error_code.len = 1;
                m_instance_device.error_code.val.p_int32[0] = 0;

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);
                break;
            }

            default:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
                return 0;
            }
        }
    }
    else if (op_code == LWM2M_OPERATION_CODE_OBSERVE)
    {
        // Already handled
    }
    else
    {
        (void)lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
    }

    return err_code;
}

lwm2m_device_t * lwm2m_device_get_instance(uint16_t instance_id)
{
    return &m_instance_device;
}

lwm2m_object_t * lwm2m_device_get_object(void)
{
    return &m_object_device;
}

void lwm2m_device_init(void)
{
    lwm2m_instance_device_init(&m_instance_device);

    m_object_device.object_id = LWM2M_OBJ_DEVICE;
    m_instance_device.proto.expire_time = 60; // Default to 60 second notifications.
    (void)at_read_manufacturer(&m_instance_device.manufacturer);
    (void)at_read_model_number(&m_instance_device.model_number);

    m_instance_device.serial_number.p_val = lwm2m_imei_get();
    m_instance_device.serial_number.len = strlen(m_instance_device.serial_number.p_val);

    m_instance_device.firmware_version.len = sizeof(nrf_dfu_fw_version_t);
    m_instance_device.firmware_version.p_val = lwm2m_os_malloc(m_instance_device.firmware_version.len);

    int err = dfusock_init();
    if (err)
    {
        return;
    }

    err = dfusock_version_get(m_instance_device.firmware_version.p_val,
                              m_instance_device.firmware_version.len);
    if (err)
    {
        return;
    }

    // Declaration of default resource values.
    uint8_t power_sources[] = { LWM2M_CARRIER_POWER_SOURCE_DC };

    // Assignment of default values to Device object resources.
    lwm2m_device_current_time_update();
    lwm2m_device_utc_offset_update();
    lwm2m_device_timezone_update();
    (void)lwm2m_carrier_avail_power_sources_set(power_sources, ARRAY_SIZE(power_sources));
    (void)lwm2m_carrier_power_source_voltage_set(LWM2M_CARRIER_POWER_SOURCE_DC, 0);
    (void)lwm2m_carrier_power_source_current_set(LWM2M_CARRIER_POWER_SOURCE_DC, 0);
    (void)lwm2m_carrier_battery_level_set(0);
    (void)lwm2m_carrier_memory_total_set(0);
    m_instance_device.memory_free = 0;
    (void)lwm2m_carrier_error_code_add(LWM2M_CARRIER_ERROR_CODE_NO_ERROR);
    (void)lwm2m_bytebuffer_to_string("UQS", 3, &m_instance_device.supported_bindings);
    (void)lwm2m_carrier_device_type_set("Smart Device");
    (void)lwm2m_carrier_software_version_set("LwM2M 0.8.1");
    (void)lwm2m_carrier_hardware_version_set("1.0");
    (void)lwm2m_carrier_battery_status_set(LWM2M_CARRIER_BATTERY_STATUS_NOT_INSTALLED);

    m_instance_device.proto.callback = device_instance_callback;

    // Verizon specific SIM ICCID
    m_verizon_resources[0].len = 20;
    m_verizon_resources[0].p_val = lwm2m_os_malloc(m_verizon_resources[0].len);
    (void)at_read_sim_iccid(m_verizon_resources[0].p_val, &m_verizon_resources[0].len);

    // nRF9160 does not support Roaming in VZW, so this is always Home.
    (void)lwm2m_bytebuffer_to_string("Home", 4, &m_verizon_resources[1]);

    // Set bootstrap server as owner.
    (void)lwm2m_acl_permissions_init((lwm2m_instance_t *)&m_instance_device,
                                     LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

    // Set default access to LWM2M_PERMISSION_READ.
    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    LWM2M_PERMISSION_READ,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    101);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    102);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_device);

}

void lwm2m_device_notify_resource(uint16_t resource_id)
{
    coap_observer_t * p_observer = NULL;
    while (coap_observe_server_next_get(&p_observer, p_observer, (void *)&m_instance_device.resource_ids[resource_id]) == 0)
    {
        LWM2M_TRC("Observer found");
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);
        uint32_t err_code = lwm2m_tlv_device_encode(buffer,
                                                      &buffer_size,
                                                      resource_id,
                                                      &m_instance_device);
        if (err_code)
        {
            LWM2M_ERR("Could not encode resource_id %u, error code: %lu", resource_id, err_code);
        }

        coap_msg_type_t type = COAP_TYPE_NON;
        int64_t now = lwm2m_os_uptime_get();

        // Send CON every configured interval
        if ((m_con_time_start[resource_id] + (lwm2m_coap_con_interval_get() * 1000)) < now) {
            type = COAP_TYPE_CON;
            m_con_time_start[resource_id] = now;
        }

        LWM2M_INF("Notify /3/0/%d", resource_id);
        err_code =  lwm2m_notify(buffer,
                                 buffer_size,
                                 p_observer,
                                 type);
        if (err_code)
        {
            LWM2M_INF("Notify /3/0/%d failed: %s (%ld), %s (%d)", resource_id,
                      lwm2m_os_log_strdup(strerror(err_code)), err_code,
                      lwm2m_os_log_strdup(lwm2m_os_strerror()), lwm2m_os_errno());

            lwm2m_request_remote_reconnect(p_observer->remote);
        }
    }
}
