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
#include <lwm2m_remote.h>
#include <lwm2m_os.h>
#include <coap_message.h>
#include <lwm2m_common.h>
#include <at_interface.h>
#include <lwm2m_carrier_main.h>
#include <operator_check.h>
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

static bool operation_is_allowed(uint16_t res, uint16_t op)
{
    if (res < ARRAY_SIZE(m_instance_device.operations)) {
        return m_instance_device.operations[res] & op;
    }

    /* Allow by default, it could be a carrier-specific resource */
    return true;
}

static uint32_t tlv_device_verizon_encode(uint8_t * p_buffer, uint32_t * p_buffer_len)
{
    lwm2m_list_t list =
    {
        .type         = LWM2M_LIST_TYPE_STRING,
        .val.p_string = m_verizon_resources,
        .len          = 2,
        .max_len      = ARRAY_SIZE(m_verizon_resources)
    };

    return lwm2m_tlv_list_encode(p_buffer, p_buffer_len, VERIZON_RESOURCE, &list);
}

static void lwm2m_device_time_resources_update(void)
{
    char string_buffer[17];

    lwm2m_time_t time;
    int utc_offset;
    const char *p_tz;

    lwm2m_carrier_time_read(&time, &utc_offset, &p_tz);

    // Update UTC time
    m_instance_device.current_time = time;

    // Update UTC offset
    snprintf(string_buffer, sizeof(string_buffer), "UTC%+03d:%02d", utc_offset / 60, abs(utc_offset % 60));

    (void)lwm2m_bytebuffer_to_string(string_buffer, strlen(string_buffer), &m_instance_device.utc_offset);

    // Update timezone
    int tz_len = strlen(p_tz);

    if (tz_len > MAX_TIMEZONE_LEN)
    {
        tz_len = MAX_TIMEZONE_LEN;
    }

    (void)lwm2m_bytebuffer_to_string((char*)p_tz, tz_len, &m_instance_device.timezone);
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

int32_t lwm2m_device_battery_status_get(void)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    // Iterate the list of available power sources to verify that an Internal Battery (1) is present.
    for (int i = 0; i < (device_obj_instance->avail_power_sources.len); i++)
    {
        if (device_obj_instance->avail_power_sources.val.p_uint8[i] == (uint8_t)LWM2M_CARRIER_POWER_SOURCE_INTERNAL_BATTERY)
        {
            return device_obj_instance->battery_status;
        }
    }
    return LWM2M_CARRIER_BATTERY_STATUS_NOT_INSTALLED;
}

static void on_read(uint16_t res, coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;

    len = sizeof(buf);

    if (res == LWM2M_NAMED_OBJECT) {
        LWM2M_INF("Read /3/0");
    } else {
        LWM2M_INF("Read /3/0/%d", res);
    }

    if (res == VERIZON_RESOURCE && operator_is_vzw(true)) {
        err = tlv_device_verizon_encode(buf, &len);
        if (err) {
            goto reply_error;
        }

        lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
        return;
    }

    /* Update requested resource */
    switch (res) {
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
        lwm2m_device_time_resources_update();
        break;
    }

    err = lwm2m_tlv_device_encode(buf, &len, res, &m_instance_device);
    if (err) {
        goto reply_error;
    }

    /* Append VzW resource */
    if (res == LWM2M_NAMED_OBJECT && operator_is_vzw(true)) {
        size_t plen = sizeof(buf) - len;
        err = tlv_device_verizon_encode(buf + len, &plen);
        if (err) {
           goto reply_error;
        }
        len += plen;
    }

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
    return;

reply_error: {
        const coap_msg_code_t code =
                (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                                   COAP_CODE_500_INTERNAL_SERVER_ERROR;

        lwm2m_respond_with_code(code, p_req);
    }
}

static void on_observe_start(uint16_t res, coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;

    const uint16_t path[] = { LWM2M_OBJ_DEVICE, 0, res };

    LWM2M_INF("Observe register /3/0/%d", res);

    err = lwm2m_tlv_device_encode(buf, &len, res, &m_instance_device);
    if (err) {
        const coap_msg_code_t code =
            (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                               COAP_CODE_400_BAD_REQUEST;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    err = lwm2m_observe_register(
            buf, len, m_instance_device.proto.expire_time, p_req,
            COAP_CT_APP_LWM2M_TLV, res, &m_instance_device.proto);

    if (err) {
        LWM2M_WRN("Failed to register observer, err %d", err);
        return;
    }

    m_con_time_start[res] = lwm2m_os_uptime_get();
    err = lwm2m_observable_metadata_init(p_req->remote, path, ARRAY_SIZE(path));
    if (err) {
        /* Already logged */
    }

    return;
}

static void on_observe_stop(uint16_t res, coap_message_t *p_req)
{
    uint32_t err;
    const uint16_t path[] = { LWM2M_OBJ_DEVICE, 0, res };

    LWM2M_INF("Observe deregister /3/0/%d", res);

    err = lwm2m_observe_unregister(p_req->remote,
                                   &m_instance_device.resource_ids[res]);

    if (err) {
        /* TODO */
    }

    lwm2m_notif_attr_storage_update(path, ARRAY_SIZE(path), p_req->remote);

    /* Process as a read */
    on_read(res, p_req);
}

static void on_observe(uint16_t res, coap_message_t *p_req)
{
    uint32_t err = 1;
    uint32_t opt;

    for (uint8_t i = 0; i < p_req->options_count; i++) {
        if (p_req->options[i].number == COAP_OPT_OBSERVE) {
            err = coap_opt_uint_decode(&opt,
                           p_req->options[i].length,
                           p_req->options[i].data);
            break;
        }
    }

    if (err) {
        lwm2m_respond_with_code(COAP_CODE_402_BAD_OPTION, p_req);
        return;
    }

    switch (opt) {
    case 0: /* observe start */
        on_observe_start(res, p_req);
        break;
    case 1: /* observe stop */
        on_observe_stop(res, p_req);
        break;
    default:
        /* Unexpected opt value */
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        break;
    }
}

static void on_write_attribute(uint16_t res, coap_message_t *p_req)
{
    int err;
    const uint16_t path[] = { LWM2M_OBJ_DEVICE, 0, res };

    const uint16_t len =
        (res == LWM2M_NAMED_OBJECT) ? ARRAY_SIZE(path) - 1 :
                                      ARRAY_SIZE(path);

    err = lwm2m_write_attribute_handler(path, len, p_req);
    if (err) {
        const coap_msg_code_t code =
            (err == -EINVAL) ? COAP_CODE_400_BAD_REQUEST :
                               COAP_CODE_500_INTERNAL_SERVER_ERROR;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_write(uint16_t res, coap_message_t *p_req)
{
    uint32_t err;
    uint32_t mask;

    if (res == LWM2M_NAMED_OBJECT) {
        LWM2M_INF("Write /3/0");
    } else {
        LWM2M_INF("Write /3/0/%d", res);
    }

    err = coap_message_ct_mask_get(p_req, &mask);
    if (err) {
        lwm2m_respond_with_code(COAP_CODE_400_BAD_REQUEST, p_req);
        return;
    }

    if (mask & COAP_CT_MASK_APP_LWM2M_TLV) {
        /* Decode TLV payload */
        err = lwm2m_tlv_device_decode(&m_instance_device,
                                      p_req->payload,
                                      p_req->payload_len,
                                      NULL);

    } else if ((mask & COAP_CT_MASK_PLAIN_TEXT) ||
               (mask & COAP_CT_MASK_APP_OCTET_STREAM)) {
        /* Decode plaintext / octect stream payload */
        err = lwm2m_plain_text_device_decode(&m_instance_device,
                                             res, p_req->payload,
                                             p_req->payload_len);
    } else {
        lwm2m_respond_with_code(COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT, p_req);
        return;
    }

    if (err) {
        /* Failed to decode or to process the payload.
         * We attempted to decode a resource and failed because
         * - memory contraints or
         * - the payload contained unexpected data
         */
        const coap_msg_code_t code =
            (err == ENOTSUP) ? COAP_CODE_404_NOT_FOUND :
                               COAP_CODE_400_BAD_REQUEST;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    /* Trigger after decoding */
    switch (res) {
    case LWM2M_DEVICE_CURRENT_TIME:
        err = lwm2m_carrier_utc_time_write(m_instance_device.current_time);
        break;
    case LWM2M_DEVICE_UTC_OFFSET:
        err = lwm2m_device_utc_offset_write(&m_instance_device);
        break;
    case LWM2M_DEVICE_TIMEZONE:
        lwm2m_device_timezone_write(&m_instance_device);
        break;
    case LWM2M_NAMED_OBJECT:
        err = lwm2m_carrier_utc_time_write(m_instance_device.current_time);
        if (err) {
            break;
        }
        err = lwm2m_device_utc_offset_write(&m_instance_device);
        if (err) {
            break;
        }
        lwm2m_device_timezone_write(&m_instance_device);
        break;
    }

    const coap_msg_code_t code =
        (err) ? COAP_CODE_400_BAD_REQUEST:
                COAP_CODE_204_CHANGED;

    lwm2m_respond_with_code(code, p_req);
}

static void on_exec(uint16_t res, coap_message_t *p_req)
{
    int err;

    LWM2M_INF("Execute /3/0/%d", res);

    switch (res) {
    case LWM2M_DEVICE_FACTORY_RESET:
        lwm2m_factory_reset();
        /* FALLTHROUGH */

    case LWM2M_DEVICE_REBOOT:
        err = lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
        if (err) {
            break;
        }

        /* FIXME:
         * This sleep is needed to ensure the response
         * is sent before closing the socket.
         */
        lwm2m_os_sleep(K_SECONDS(1));

        lwm2m_request_reset();
        break;

    case LWM2M_DEVICE_RESET_ERROR_CODE:
        m_instance_device.error_code.len = 1;
        m_instance_device.error_code.val.p_int32[0] = 0;

        (void) lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
        break;
    }
}

static void on_discover(uint16_t res, coap_message_t *p_req)
{
    uint32_t err;

    err = lwm2m_respond_with_instance_link(&m_instance_device.proto, res, p_req);
    if (err) {
        LWM2M_WRN("Failed to respond to discover on /3/0/%d, err %d", res, err);
    }
}

uint32_t device_instance_callback(lwm2m_instance_t *p_instance,
                                  uint16_t resource_id, uint8_t op_code,
                                  coap_message_t *p_request)
{
    uint16_t access;
    uint32_t err_code;

    err_code = lwm2m_access_remote_get(&access, p_instance, p_request->remote);
    if (err_code != 0) {
        return err_code;
    }

    /* Check server access */
    op_code = (access & op_code);
    if (op_code == 0) {
        lwm2m_respond_with_code(COAP_CODE_401_UNAUTHORIZED, p_request);
        return 0;
    }

    /* Check resource permissions */
    if (!operation_is_allowed(resource_id, op_code)) {
        LWM2M_WRN("Operation 0x%x on resource 3/0/%d, not allowed",
                  op_code, resource_id);
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        return 0;
    }

    if (p_instance->instance_id != 0) {
        lwm2m_respond_with_code(COAP_CODE_404_NOT_FOUND, p_request);
        return 0;
    }

    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_read(resource_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE:
        on_write(resource_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_EXECUTE:
        on_exec(resource_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_OBSERVE:
        on_observe(resource_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_discover(resource_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_write_attribute(resource_id, p_request);
        break;
    default:
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        break;
    }

    return 0;
}

static void on_object_read(coap_message_t *p_req)
{
    uint32_t err;
    uint8_t buf[256];
    size_t len;

    len = sizeof(buf);

    err = lwm2m_tlv_device_encode(buf + 3, &len, LWM2M_NAMED_OBJECT,
                                  &m_instance_device);

    if (err) {
        /* TODO */
        return;
    }

    lwm2m_tlv_t tlv = {
        .id_type = TLV_TYPE_OBJECT,
        .length = len
    };

    err = lwm2m_tlv_header_encode(buf, &len, &tlv);
    if (err) {
        /* TODO */
        return;
    }

    len += tlv.length;

    lwm2m_respond_with_payload(buf, len, COAP_CT_APP_LWM2M_TLV, p_req);
}

static void on_object_write_attribute(uint16_t instance, coap_message_t *p_req)
{
    int err;
    uint16_t path[] = { LWM2M_OBJ_DEVICE };

    err = lwm2m_write_attribute_handler(path, ARRAY_SIZE(path), p_req);
    if (err) {
        const coap_msg_code_t code =
            (err == -EINVAL) ? COAP_CODE_400_BAD_REQUEST :
                               COAP_CODE_500_INTERNAL_SERVER_ERROR;
        lwm2m_respond_with_code(code, p_req);
        return;
    }

    lwm2m_respond_with_code(COAP_CODE_204_CHANGED, p_req);
}

static void on_object_discover(coap_message_t * p_req)
{
    int err;

    err = lwm2m_respond_with_object_link(LWM2M_OBJ_DEVICE, p_req);
    if (err) {
        LWM2M_WRN("Failed to discover device object, err %d", err);
    }
}

/**@brief Callback function for LWM2M device objects. */
uint32_t lwm2m_device_object_callback(lwm2m_object_t * p_object,
                                      uint16_t         instance_id,
                                      uint8_t          op_code,
                                      coap_message_t * p_request)
{
    switch (op_code) {
    case LWM2M_OPERATION_CODE_READ:
        on_object_read(p_request);
        break;
    case LWM2M_OPERATION_CODE_WRITE_ATTR:
        on_object_write_attribute(instance_id, p_request);
        break;
    case LWM2M_OPERATION_CODE_DISCOVER:
        on_object_discover(p_request);
        break;
    default:
        lwm2m_respond_with_code(COAP_CODE_405_METHOD_NOT_ALLOWED, p_request);
        break;
    }

    return 0;
}

lwm2m_device_t * lwm2m_device_get_instance(uint16_t instance_id)
{
    return &m_instance_device;
}

lwm2m_object_t * lwm2m_device_get_object(void)
{
    return &m_object_device;
}

void lwm2m_device_update_carrier_specific_settings(void)
{
    if (operator_is_att(true))
    {
        char svn[3];
        at_read_svn(svn, sizeof(svn));

        (void)lwm2m_carrier_device_type_set("Module - LGA");
        (void)lwm2m_carrier_software_version_set(svn);
    }
    else
    {
        (void)lwm2m_carrier_device_type_set("Smart Device");
        (void)lwm2m_carrier_software_version_set("LwM2M 0.9");
    }
}

void lwm2m_device_init_acl(void)
{
    lwm2m_set_carrier_acl((lwm2m_instance_t *)&m_instance_device);
}

void lwm2m_device_init(void)
{
    lwm2m_instance_device_init(&m_instance_device);

    m_object_device.object_id = LWM2M_OBJ_DEVICE;
    m_object_device.callback = lwm2m_device_object_callback;
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
    lwm2m_device_time_resources_update();
    (void)lwm2m_carrier_avail_power_sources_set(power_sources, ARRAY_SIZE(power_sources));
    (void)lwm2m_carrier_power_source_voltage_set(LWM2M_CARRIER_POWER_SOURCE_DC, 0);
    (void)lwm2m_carrier_power_source_current_set(LWM2M_CARRIER_POWER_SOURCE_DC, 0);
    (void)lwm2m_carrier_battery_level_set(0);
    (void)lwm2m_carrier_memory_total_set(0);
    m_instance_device.memory_free = 0;
    (void)lwm2m_carrier_error_code_add(LWM2M_CARRIER_ERROR_CODE_NO_ERROR);
    (void)lwm2m_bytebuffer_to_string("UQS", 3, &m_instance_device.supported_bindings);
    lwm2m_device_update_carrier_specific_settings();
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

    lwm2m_device_init_acl();

    (void)lwm2m_coap_handler_instance_add((lwm2m_instance_t *)&m_instance_device);
}

const void * lwm2m_device_resource_reference_get(uint16_t resource_id, uint8_t *p_type)
{
    // TODO: Assert type != NULL
    switch (resource_id)
    {
    case LWM2M_DEVICE_AVAILABLE_POWER_SOURCES:
        *p_type = LWM2M_OBSERVABLE_TYPE_LIST;
        return &m_instance_device.avail_power_sources;
    case LWM2M_DEVICE_POWER_SOURCE_VOLTAGE:
        *p_type = LWM2M_OBSERVABLE_TYPE_LIST;
        return &m_instance_device.power_source_voltage;
    case LWM2M_DEVICE_POWER_SOURCE_CURRENT:
        *p_type = LWM2M_OBSERVABLE_TYPE_LIST;
        return &m_instance_device.power_source_current;
    case LWM2M_DEVICE_ERROR_CODE:
        *p_type = LWM2M_OBSERVABLE_TYPE_INT;
        return &m_instance_device.error_code;
    case LWM2M_DEVICE_DEVICE_TYPE:
        *p_type = LWM2M_OBSERVABLE_TYPE_STR;
        return &m_instance_device.device_type;
    case LWM2M_DEVICE_HARDWARE_VERSION:
        *p_type = LWM2M_OBSERVABLE_TYPE_STR;
        return &m_instance_device.hardware_version;
    case LWM2M_DEVICE_SOFTWARE_VERSION:
        *p_type = LWM2M_OBSERVABLE_TYPE_STR;
        return &m_instance_device.software_version;
    case LWM2M_DEVICE_BATTERY_LEVEL:
        *p_type = LWM2M_OBSERVABLE_TYPE_INT;
        return &m_instance_device.battery_level;
    case LWM2M_DEVICE_SUPPORTED_BINDINGS:
        *p_type = LWM2M_OBSERVABLE_TYPE_STR;
        return &m_instance_device.supported_bindings;
    case LWM2M_DEVICE_BATTERY_STATUS:
        *p_type = LWM2M_OBSERVABLE_TYPE_INT;
        return &m_instance_device.battery_status;
    case LWM2M_DEVICE_MEMORY_TOTAL:
        *p_type = LWM2M_OBSERVABLE_TYPE_INT;
        return &m_instance_device.memory_total;
    default:
        *p_type = LWM2M_OBSERVABLE_TYPE_NO_CHECK;
        return NULL;
    }
}

void lwm2m_device_notify_resource(struct nrf_sockaddr * p_remote_server, uint16_t resource_id)
{
    uint16_t short_server_id;
    coap_observer_t * p_observer = NULL;

    while (coap_observe_server_next_get(&p_observer, p_observer, (void *)&m_instance_device.resource_ids[resource_id]) == 0)
    {
        lwm2m_remote_short_server_id_find(&short_server_id, p_observer->remote);
        if (lwm2m_remote_reconnecting_get(short_server_id)) {
            /* Wait for reconnection */
            continue;
        }

        if (p_remote_server != NULL) {
            /* Only notify to given remote */
            if (memcmp(p_observer->remote, p_remote_server,
                       sizeof(struct nrf_sockaddr)) != 0) {
                continue;
            }
        }

        uint32_t err_code;
        uint8_t  buffer[200];
        uint32_t buffer_size = sizeof(buffer);

        LWM2M_TRC("Observer found");
        err_code = lwm2m_tlv_device_encode(buffer, &buffer_size, resource_id, &m_instance_device);
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
