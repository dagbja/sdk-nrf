/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m_objects.h>
#include <lwm2m_os.h>
#include <lwm2m_api.h>
#include <lwm2m_device.h>
#include <lwm2m_carrier.h>
#include <stdint.h>

#define LWM2M_CARRIER_STRING_MAX_LEN 200

int lwm2m_carrier_avail_power_sources_set(const uint8_t *power_sources, uint8_t power_source_count)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if (power_source_count > LWM2M_DEVICE_MAX_POWER_SOURCES)
    {
        return -E2BIG;
    }
    // First iteration to check if the correct power source types are specified.
    for (int i = 0; i < power_source_count; i++)
    {
        if ((power_sources[i] < LWM2M_CARRIER_POWER_SOURCE_DC) || (power_sources[i] > LWM2M_CARRIER_POWER_SOURCE_SOLAR) || (power_sources[i] == 3))
        {
            return -EINVAL;
        }
    }

    device_obj_instance->avail_power_sources.len = power_source_count;
    device_obj_instance->power_source_current.len = power_source_count;
    device_obj_instance->power_source_voltage.len = power_source_count;

    for (int i = 0; i < power_source_count; i++)
    {
        device_obj_instance->avail_power_sources.val.p_uint8[i] = power_sources[i];
        device_obj_instance->power_source_current.val.p_int32[i] = 0;
        device_obj_instance->power_source_voltage.val.p_int32[i] = 0;
    }
    lwm2m_device_notify_resource(LWM2M_DEVICE_AVAILABLE_POWER_SOURCES);
    lwm2m_device_notify_resource(LWM2M_DEVICE_POWER_SOURCE_CURRENT);
    lwm2m_device_notify_resource(LWM2M_DEVICE_POWER_SOURCE_VOLTAGE);

    device_obj_instance->battery_status = LWM2M_CARRIER_BATTERY_STATUS_UNKNOWN;
    device_obj_instance->battery_level = 0;
    lwm2m_device_notify_resource(LWM2M_DEVICE_BATTERY_STATUS);
    lwm2m_device_notify_resource(LWM2M_DEVICE_BATTERY_LEVEL);

    return 0;
}

int lwm2m_carrier_power_source_voltage_set(uint8_t power_source, int32_t value)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if ((power_source < LWM2M_CARRIER_POWER_SOURCE_DC) || (power_source > LWM2M_CARRIER_POWER_SOURCE_SOLAR) || (power_source == 3))
    {
        return -EINVAL;
    }

    for (int i = 0; i < device_obj_instance->avail_power_sources.len; i++)
    {
        if (device_obj_instance->avail_power_sources.val.p_uint8[i] == power_source)
        {
            device_obj_instance->power_source_voltage.val.p_int32[i] = value;
            lwm2m_device_notify_resource(LWM2M_DEVICE_POWER_SOURCE_VOLTAGE);

            return 0;
        }
    }

    return -ENODEV;
}

int lwm2m_carrier_power_source_current_set(uint8_t power_source, int32_t value)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if ((power_source < LWM2M_CARRIER_POWER_SOURCE_DC) || (power_source > LWM2M_CARRIER_POWER_SOURCE_SOLAR) || (power_source == 3))
    {
        return -EINVAL;
    }

    for (int i = 0; i < device_obj_instance->avail_power_sources.len; i++)
    {
        if (device_obj_instance->avail_power_sources.val.p_uint8[i] == power_source)
        {
            device_obj_instance->power_source_current.val.p_int32[i] = value;
            lwm2m_device_notify_resource(LWM2M_DEVICE_POWER_SOURCE_CURRENT);

            return 0;
        }
    }

    return -ENODEV;
}

int lwm2m_carrier_battery_level_set(uint8_t battery_level)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if (battery_level > 100)
    {
        return -EINVAL;
    }
    // Iterate the list of available power sources to verify that an Internal Battery (1) is present.
    for (int i = 0; i < (device_obj_instance->avail_power_sources.len); i++)
    {
        if (device_obj_instance->avail_power_sources.val.p_uint8[i] == LWM2M_CARRIER_POWER_SOURCE_INTERNAL_BATTERY)
        {
            device_obj_instance->battery_level = battery_level;
            lwm2m_device_notify_resource(LWM2M_DEVICE_BATTERY_LEVEL);
            return 0;
        }
    }

    return -ENODEV;
}

int lwm2m_carrier_battery_status_set(int32_t battery_status)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if ((battery_status < LWM2M_CARRIER_BATTERY_STATUS_NORMAL) || (battery_status > LWM2M_CARRIER_BATTERY_STATUS_UNKNOWN))
    {
        return -EINVAL;
    }

    // Iterate the list of available power sources to verify that an Internal Battery (1) is present.
    for (int i = 0; i < (device_obj_instance->avail_power_sources.len); i++)
    {
        if (device_obj_instance->avail_power_sources.val.p_uint8[i] == LWM2M_CARRIER_POWER_SOURCE_INTERNAL_BATTERY)
        {
            device_obj_instance->battery_status = battery_status;
            lwm2m_device_notify_resource(LWM2M_DEVICE_BATTERY_STATUS);
            return 0;
        }
    }
    device_obj_instance->battery_status = LWM2M_CARRIER_BATTERY_STATUS_NOT_INSTALLED;

    return -ENODEV;
}

int lwm2m_carrier_device_type_set(const char *device_type)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    int retval = 0;

    if (device_type == NULL || device_type[0] == '\0')
    {
        return -EINVAL;
    }
    if (strlen(device_type) > LWM2M_CARRIER_STRING_MAX_LEN)
    {
        return -E2BIG;
    }

    retval = (int)lwm2m_bytebuffer_to_string(device_type, strlen(device_type), &(device_obj_instance->device_type));
    lwm2m_device_notify_resource(LWM2M_DEVICE_DEVICE_TYPE);

    return -retval;
}

int lwm2m_carrier_hardware_version_set(const char * hardware_version)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    int retval = 0;

    if (hardware_version == NULL || hardware_version[0] == '\0')
    {
        return -EINVAL;
    }
    if (strlen(hardware_version) > LWM2M_CARRIER_STRING_MAX_LEN)
    {
        return -E2BIG;
    }

    retval = (int)lwm2m_bytebuffer_to_string(hardware_version, strlen(hardware_version), &(device_obj_instance->hardware_version));
    lwm2m_device_notify_resource(LWM2M_DEVICE_HARDWARE_VERSION);

    return -retval;
}

int lwm2m_carrier_software_version_set(const char * software_version)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    int retval = 0;

    if (software_version == NULL || software_version[0] == '\0')
    {
        return -EINVAL;
    }
    if (strlen(software_version) > LWM2M_CARRIER_STRING_MAX_LEN)
    {
        return -E2BIG;
    }

    retval = (int)lwm2m_bytebuffer_to_string(software_version, strlen(software_version), &(device_obj_instance->software_version));
    lwm2m_device_notify_resource(LWM2M_DEVICE_SOFTWARE_VERSION);

    return -retval;
}

int lwm2m_carrier_error_code_add(int32_t error)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    uint32_t len = device_obj_instance->error_code.len;

    if ((error < LWM2M_CARRIER_ERROR_CODE_NO_ERROR) || (error > LWM2M_CARRIER_ERROR_CODE_PERIPHERAL_MALFUNCTION))
    {
        return -EINVAL;
    }

    if (error == LWM2M_CARRIER_ERROR_CODE_NO_ERROR)
    {
        device_obj_instance->error_code.len = 1;
        device_obj_instance->error_code.val.p_int32[0] = error;
        lwm2m_device_notify_resource(LWM2M_DEVICE_ERROR_CODE);
        return 0;
    }

    if ((len == 1) && (device_obj_instance->error_code.val.p_int32[0] == LWM2M_CARRIER_ERROR_CODE_NO_ERROR))
    {
        device_obj_instance->error_code.val.p_int32[0] = error;
        lwm2m_device_notify_resource(LWM2M_DEVICE_ERROR_CODE);
        return 0;
    }

    for (int i = 0; i < len; i++)
    {
        if (device_obj_instance->error_code.val.p_int32[i] == error)
        {
            return 0;
        }
    }

    device_obj_instance->error_code.len++;
    device_obj_instance->error_code.val.p_int32[len] = error;
    lwm2m_device_notify_resource(LWM2M_DEVICE_ERROR_CODE);

    return 0;
}

int lwm2m_carrier_error_code_remove(int32_t error)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    uint32_t len = device_obj_instance->error_code.len;

    if ((error < LWM2M_CARRIER_ERROR_CODE_NO_ERROR) || (error > LWM2M_CARRIER_ERROR_CODE_PERIPHERAL_MALFUNCTION))
    {
        return -EINVAL;
    }

    for (int i = 0; i < len; i++)
    {
        if (device_obj_instance->error_code.val.p_int32[i] == error)
        {
            if (len == 1)
            {
                device_obj_instance->error_code.val.p_int32[0] = LWM2M_CARRIER_ERROR_CODE_NO_ERROR;
                lwm2m_device_notify_resource(LWM2M_DEVICE_ERROR_CODE);
                return 0;
            }
            else
            {
                device_obj_instance->error_code.len--;

                for (int j = i; j < len; j++)
                {
                    device_obj_instance->error_code.val.p_int32[j] = device_obj_instance->error_code.val.p_int32[j + 1];
                }
                lwm2m_device_notify_resource(LWM2M_DEVICE_ERROR_CODE);
                return 0;
            }
        }
    }

    return -ENOENT;
}

int lwm2m_carrier_memory_total_set(uint32_t memory_total)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if (memory_total > INT32_MAX)
    {
        return -EINVAL;
    }
    device_obj_instance->memory_total = (int32_t)memory_total;
    lwm2m_device_notify_resource(LWM2M_DEVICE_MEMORY_TOTAL);

    return 0;
}

int __WEAK lwm2m_carrier_memory_free_read(void)
{
    return 0;
}

