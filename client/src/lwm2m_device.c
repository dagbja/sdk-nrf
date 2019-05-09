/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define LWM2M_LOG_MODULE_NAME lwm2m_device

#include <stdint.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_objects_plain_text.h>
#include <common.h>
#include <at_interface.h>
#include <main.h>

#define VERIZON_RESOURCE 30000

#define APP_MOTIVE_FAKE_POWER_SOURCES   1 // To pass MotiveBridge power source tests (4.10, 4.11 and 4.12)

static lwm2m_object_t m_object_device;    /**< Device base object. */
static lwm2m_device_t m_instance_device;  /**< Device object instance. */
static lwm2m_string_t m_verizon_resources[2];

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

    if (op_code == LWM2M_OPERATION_CODE_READ)
    {
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        if (resource_id == VERIZON_RESOURCE)
        {
            err_code = tlv_device_verizon_encode(instance_id, buffer, &buffer_size);
        }
        else
        {
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
        u32_t mask = 0;
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
            case LWM2M_DEVICE_REBOOT:
            {
                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                app_request_reboot();
                break;
            }

            case LWM2M_DEVICE_FACTORY_RESET:
            {
                app_factory_reset();

                (void)lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_request);

                app_request_reboot();
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

    m_instance_device.manufacturer.p_val = "Nordic Semiconductor";
    m_instance_device.manufacturer.len = strlen(m_instance_device.manufacturer.p_val);
    m_instance_device.model_number.p_val = "nRF91";
    m_instance_device.model_number.len = strlen(m_instance_device.model_number.p_val);
    m_instance_device.serial_number.p_val = app_imei_get();
    m_instance_device.serial_number.len = strlen(m_instance_device.serial_number.p_val);
    m_instance_device.firmware_version.p_val = "1.0";
    m_instance_device.firmware_version.len = strlen(m_instance_device.firmware_version.p_val);

#if APP_MOTIVE_FAKE_POWER_SOURCES
    m_instance_device.avail_power_sources.len = 2;
    m_instance_device.avail_power_sources.val.p_uint8[0] = 0; // DC power
    m_instance_device.avail_power_sources.val.p_uint8[1] = 2; // External Battery
    m_instance_device.power_source_voltage.len = 2;
    m_instance_device.power_source_voltage.val.p_int32[0] = 5108;
    m_instance_device.power_source_voltage.val.p_int32[1] = 5242;
    m_instance_device.power_source_current.len = 2;
    m_instance_device.power_source_current.val.p_int32[0] = 42;
    m_instance_device.power_source_current.val.p_int32[1] = 0;
#else
    m_instance_device.avail_power_sources.len = 0;
    m_instance_device.power_source_voltage.len = 0;
    m_instance_device.power_source_current.len = 0;
#endif

    m_instance_device.battery_level = 0;
    m_instance_device.memory_free = 64;
    m_instance_device.error_code.len = 1;
    m_instance_device.error_code.val.p_int32[0] = 0; // No error
    m_instance_device.current_time = 1546300800; // Tue Jan 01 00:00:00 CEST 2019
    char * utc_offset = "+02:00";
    (void)lwm2m_bytebuffer_to_string(utc_offset, strlen(utc_offset), &m_instance_device.utc_offset);
    char * timezone = "Europe/Oslo";
    (void)lwm2m_bytebuffer_to_string(timezone, strlen(timezone),&m_instance_device.timezone);
    (void)lwm2m_bytebuffer_to_string("UQS", 3, &m_instance_device.supported_bindings);
    m_instance_device.device_type.p_val = "Smart Device";
    m_instance_device.device_type.len = strlen(m_instance_device.device_type.p_val);
    m_instance_device.hardware_version.p_val = "1.0";
    m_instance_device.hardware_version.len = strlen(m_instance_device.hardware_version.p_val);
    m_instance_device.software_version.p_val = "1.0";
    m_instance_device.software_version.len = strlen(m_instance_device.software_version.p_val);
    m_instance_device.battery_status = 5;
    m_instance_device.memory_total = 128;

    m_instance_device.proto.callback = device_instance_callback;

    // Verizon specific SIM ICCID and HomeOrRoaming
    m_verizon_resources[0].len = 20;
    m_verizon_resources[0].p_val = lwm2m_malloc(m_verizon_resources[0].len);
    (void)at_read_sim_iccid(m_verizon_resources[0].p_val, &m_verizon_resources[0].len);
    char * home_or_roaming = "Home"; // TODO: Read from AT+CEREG?
    (void)lwm2m_bytebuffer_to_string(home_or_roaming, strlen(home_or_roaming), &m_verizon_resources[1]);

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
                                    102);

    (void)lwm2m_acl_permissions_add((lwm2m_instance_t *)&m_instance_device,
                                    (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE |
                                     LWM2M_PERMISSION_OBSERVE),
                                    1000);

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_device);

}
