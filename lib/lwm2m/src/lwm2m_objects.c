/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <stddef.h>

#include <lwm2m.h>
#include <lwm2m_objects.h>

// Access Control object
static uint16_t       m_servers[LWM2M_ACCESS_CONTROL_MAX_INSTANCES][LWM2M_MAX_SERVERS + 1];
static uint16_t       m_acl[LWM2M_ACCESS_CONTROL_MAX_INSTANCES][LWM2M_MAX_SERVERS + 1];

// Connectivity Monitoring object
static int32_t        m_available_network_bearer[LWM2M_CONNECTIVITY_MONITORING_MAX_NETWORK_BEARERS];
static lwm2m_string_t m_ip_addresses[LWM2M_CONNECTIVITY_MONITORING_MAX_IP_ADDRESSES];
static lwm2m_string_t m_router_ip_addresses[LWM2M_CONNECTIVITY_MONITORING_MAX_IP_ADDRESSES];
static lwm2m_string_t m_apn[LWM2M_CONNECTIVITY_MONITORING_MAX_APNS];

// Device object
static uint8_t        m_avail_power_sources[LWM2M_DEVICE_MAX_POWER_SOURCES];
static int32_t        m_power_source_voltage[LWM2M_DEVICE_MAX_POWER_SOURCES];
static int32_t        m_power_source_current[LWM2M_DEVICE_MAX_POWER_SOURCES];
static int32_t        m_ext_dev_info[LWM2M_DEVICE_MAX_DEV_INFO];
static int32_t        m_error_codes[LWM2M_DEVICE_MAX_ERROR_CODES];

// APN Connection Profile object
static int32_t        m_conn_est_time[LWM2M_MAX_APN_COUNT][LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS];
static uint8_t        m_conn_est_result[LWM2M_MAX_APN_COUNT][LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS];
static uint8_t        m_conn_est_reject_cause[LWM2M_MAX_APN_COUNT][LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS];
static int32_t        m_conn_end_time[LWM2M_MAX_APN_COUNT][LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS];

// AT&T Connectivity extension object
static uint8_t        m_apn_retries[LWM2M_MAX_APN_COUNT];
static int32_t        m_apn_retry_period[LWM2M_MAX_APN_COUNT];
static int32_t        m_apn_retry_back_off_period[LWM2M_MAX_APN_COUNT];

// lint -e516 -save // Symbol '__INTADDR__()' has arg. type conflict
#define LWM2M_INSTANCE_OFFSET_SET(instance, type)                     \
    instance->proto.operations_offset   = offsetof(type, operations); \
    instance->proto.resource_ids_offset = offsetof(type, resource_ids);
// lint -restore

/* Only in this file, use shorter versions of these defines for readability */
#define NONE     LWM2M_OPERATION_CODE_NONE
#define WRITE    LWM2M_OPERATION_CODE_WRITE
#define READ     LWM2M_OPERATION_CODE_READ
#define EXEC     LWM2M_OPERATION_CODE_EXECUTE
#define OBSV     LWM2M_OPERATION_CODE_OBSERVE
#define DISC     LWM2M_OPERATION_CODE_DISCOVER
#define WRATT    LWM2M_OPERATION_CODE_WRITE_ATTR

uint32_t lwm2m_bytebuffer_to_string(const char * p_payload, uint16_t payload_len, lwm2m_string_t * p_string)
{
    NULL_PARAM_CHECK(p_payload);
    NULL_PARAM_CHECK(p_string);

    char * p_value = (char *)lwm2m_malloc(payload_len);

    if (p_value == NULL)
    {
        return ENOMEM;
    }

    memcpy(p_value, p_payload, payload_len);

    if (p_string->p_val)
    {
        lwm2m_free(p_string->p_val);
    }

    p_string->p_val = p_value;
    p_string->len   = payload_len;

    return 0;
}

uint32_t lwm2m_bytebuffer_to_opaque(const char * p_payload, uint16_t payload_len, lwm2m_opaque_t * p_opaque)
{
    return lwm2m_bytebuffer_to_string(p_payload, payload_len, (lwm2m_string_t *)p_opaque);
}

uint32_t lwm2m_bytebuffer_to_list(const char * p_payload, uint16_t payload_len, lwm2m_list_t * p_list)
{
    NULL_PARAM_CHECK(p_payload);
    NULL_PARAM_CHECK(p_list);

    char * p_value = (char *)lwm2m_malloc(payload_len);

    if (p_value == NULL)
    {
        return ENOMEM;
    }

    memcpy(p_value, p_payload, payload_len);

    if (p_list->val.p_uint8)
    {
        lwm2m_free(p_list->val.p_uint8);
    }

    p_list->val.p_uint8 = p_value;
    p_list->len         = payload_len;

    return 0;
}

uint32_t lwm2m_string_free(lwm2m_string_t * p_string)
{
    if (p_string->p_val)
    {
        lwm2m_free(p_string->p_val);
    }

    p_string->p_val = NULL;
    p_string->len   = 0;

    return 0;
}

uint32_t lwm2m_opaque_free(lwm2m_opaque_t * p_opaque)
{
    return lwm2m_string_free((lwm2m_string_t *)p_opaque);
}

void lwm2m_instance_security_init(lwm2m_security_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_security_t);

    p_instance->proto.object_id     = LWM2M_OBJ_SECURITY;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_security_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = NONE;
    p_instance->operations[1]  = NONE;
    p_instance->operations[2]  = NONE;
    p_instance->operations[3]  = NONE;
    p_instance->operations[4]  = NONE;
    p_instance->operations[5]  = NONE;
    p_instance->operations[6]  = NONE;
    p_instance->operations[7]  = NONE;
    p_instance->operations[8]  = NONE;
    p_instance->operations[9]  = NONE;
    p_instance->operations[10] = NONE;
    p_instance->operations[11] = NONE;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_SECURITY_SERVER_URI;
    p_instance->resource_ids[1]  = LWM2M_SECURITY_BOOTSTRAP_SERVER;
    p_instance->resource_ids[2]  = LWM2M_SECURITY_SECURITY_MODE;
    p_instance->resource_ids[3]  = LWM2M_SECURITY_PUBLIC_KEY;
    p_instance->resource_ids[4]  = LWM2M_SECURITY_SERVER_PUBLIC_KEY;
    p_instance->resource_ids[5]  = LWM2M_SECURITY_SECRET_KEY;
    p_instance->resource_ids[6]  = LWM2M_SECURITY_SMS_SECURITY_MODE;
    p_instance->resource_ids[7]  = LWM2M_SECURITY_SMS_BINDING_KEY_PARAM;
    p_instance->resource_ids[8]  = LWM2M_SECURITY_SMS_BINDING_SECRET_KEY;
    p_instance->resource_ids[9]  = LWM2M_SECURITY_SERVER_SMS_NUMBER;
    p_instance->resource_ids[10] = LWM2M_SECURITY_SHORT_SERVER_ID;
    p_instance->resource_ids[11] = LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME;
}


void lwm2m_instance_server_init(lwm2m_server_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_server_t);

    p_instance->proto.object_id     = LWM2M_OBJ_SERVER;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_server_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT;
    p_instance->operations[1] = READ | WRATT | WRITE;
    p_instance->operations[2] = READ | WRATT | WRITE;
    p_instance->operations[3] = READ | WRATT | WRITE;
    p_instance->operations[4] = EXEC;
    p_instance->operations[5] = READ | WRATT | WRITE;
    p_instance->operations[6] = READ | WRATT | WRITE;
    p_instance->operations[7] = READ | WRATT | WRITE;
    p_instance->operations[8] = EXEC;
    p_instance->operations[9] = EXEC;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_SERVER_SHORT_SERVER_ID;
    p_instance->resource_ids[1] = LWM2M_SERVER_LIFETIME;
    p_instance->resource_ids[2] = LWM2M_SERVER_DEFAULT_MIN_PERIOD;
    p_instance->resource_ids[3] = LWM2M_SERVER_DEFAULT_MAX_PERIOD;
    p_instance->resource_ids[4] = LWM2M_SERVER_DISABLE;
    p_instance->resource_ids[5] = LWM2M_SERVER_DISABLE_TIMEOUT;
    p_instance->resource_ids[6] = LWM2M_SERVER_NOTIFY_WHEN_DISABLED;
    p_instance->resource_ids[7] = LWM2M_SERVER_BINDING;
    p_instance->resource_ids[8] = LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER;
    p_instance->resource_ids[9] = LWM2M_SERVER_BOOTSTRAP_REQUEST_TRIGGER;
}


void lwm2m_instance_access_control_init(lwm2m_access_control_t * p_instance, uint16_t instance_id)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_access_control_t);

    p_instance->proto.object_id     = LWM2M_OBJ_ACCESS_CONTROL;
    p_instance->proto.instance_id   = instance_id;
    p_instance->proto.num_resources = sizeof(((lwm2m_access_control_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = READ | DISC;
    p_instance->operations[1]  = READ | DISC;
    p_instance->operations[2]  = READ | DISC | WRITE;
    p_instance->operations[3]  = READ | DISC | WRITE;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_ACCESS_CONTROL_OBJECT_ID;
    p_instance->resource_ids[1]  = LWM2M_ACCESS_CONTROL_INSTANCE_ID;
    p_instance->resource_ids[2]  = LWM2M_ACCESS_CONTROL_ACL;
    p_instance->resource_ids[3]  = LWM2M_ACCESS_CONTROL_CONTROL_OWNER;

    // ACL per server plus default
    p_instance->acl.type = LWM2M_LIST_TYPE_UINT16;
    p_instance->acl.max_len = LWM2M_MAX_SERVERS + 1;
    p_instance->acl.len = 0;
    p_instance->acl.p_id = m_servers[instance_id];
    p_instance->acl.val.p_uint16 = m_acl[instance_id];
}


void lwm2m_instance_firmware_init(lwm2m_firmware_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_firmware_t);

    p_instance->proto.object_id     = LWM2M_OBJ_FIRMWARE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_firmware_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = WRITE;
    p_instance->operations[1] = WRITE;
    p_instance->operations[2] = EXEC;
    p_instance->operations[3] = READ | WRATT | DISC | OBSV;

     // "Update Supported Objects" is not available anymore, but reserved.
    p_instance->operations[4] = NONE;

    p_instance->operations[5] = READ | WRATT | DISC | OBSV;
    p_instance->operations[6] = READ | WRATT | DISC;
    p_instance->operations[7] = READ | WRATT | DISC;
    p_instance->operations[8] = READ | WRATT | DISC;
    p_instance->operations[9] = READ | WRATT | DISC;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_FIRMWARE_PACKAGE;
    p_instance->resource_ids[1] = LWM2M_FIRMWARE_PACKAGE_URI;
    p_instance->resource_ids[2] = LWM2M_FIRMWARE_UPDATE;
    p_instance->resource_ids[3] = LWM2M_FIRMWARE_STATE;
    p_instance->resource_ids[4] = LWM2M_FIRMWARE_LEGACY_DO_NOT_RENDER;
    p_instance->resource_ids[5] = LWM2M_FIRMWARE_UPDATE_RESULT;
    p_instance->resource_ids[6] = LWM2M_FIRMWARE_PKG_NAME;
    p_instance->resource_ids[7] = LWM2M_FIRMWARE_PKG_VERSION;
    p_instance->resource_ids[8] = LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT;
    p_instance->resource_ids[9] = LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD;

    p_instance->firmware_update_protocol_support.type        = LWM2M_LIST_TYPE_UINT8;
    p_instance->firmware_update_protocol_support.max_len     = 1;
    p_instance->firmware_update_protocol_support.val.p_uint8 = NULL;
    p_instance->firmware_update_protocol_support.len         = 0;
}


void lwm2m_instance_connectivity_monitoring_init(lwm2m_connectivity_monitoring_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_connectivity_monitoring_t);

    p_instance->proto.object_id     = LWM2M_OBJ_CONN_MON;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_connectivity_monitoring_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[1]  = READ | WRATT | DISC;
    p_instance->operations[2]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[3]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[4]  = READ | WRATT | DISC;
    p_instance->operations[5]  = READ | WRATT | DISC;
    p_instance->operations[6]  = NONE; // "Link Utilization" is currently unused.
    p_instance->operations[7]  = READ | WRATT | DISC;
    p_instance->operations[8]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[9]  = READ | WRATT | DISC;
    p_instance->operations[10] = READ | WRATT | DISC;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_CONN_MON_NETWORK_BEARER;
    p_instance->resource_ids[1]  = LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER;
    p_instance->resource_ids[2]  = LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH;
    p_instance->resource_ids[3]  = LWM2M_CONN_MON_LINK_QUALITY;
    p_instance->resource_ids[4]  = LWM2M_CONN_MON_IP_ADDRESSES;
    p_instance->resource_ids[5]  = LWM2M_CONN_MON_ROUTER_IP_ADRESSES;
    p_instance->resource_ids[6]  = LWM2M_CONN_MON_LINK_UTILIZATION;
    p_instance->resource_ids[7]  = LWM2M_CONN_MON_APN;
    p_instance->resource_ids[8]  = LWM2M_CONN_MON_CELL_ID;
    p_instance->resource_ids[9]  = LWM2M_CONN_MON_SMNC;
    p_instance->resource_ids[10] = LWM2M_CONN_MON_SMCC;

    // Setup lists.
    p_instance->available_network_bearer.type         = LWM2M_LIST_TYPE_INT32;
    p_instance->available_network_bearer.p_id         = NULL;
    p_instance->available_network_bearer.val.p_int32  = m_available_network_bearer;
    p_instance->available_network_bearer.max_len      = LWM2M_CONNECTIVITY_MONITORING_MAX_NETWORK_BEARERS;

    p_instance->ip_addresses.type                     = LWM2M_LIST_TYPE_STRING;
    p_instance->ip_addresses.p_id                     = NULL;
    p_instance->ip_addresses.val.p_string             = m_ip_addresses;
    p_instance->ip_addresses.max_len                  = LWM2M_CONNECTIVITY_MONITORING_MAX_IP_ADDRESSES;

    p_instance->router_ip_addresses.type              = LWM2M_LIST_TYPE_STRING;
    p_instance->router_ip_addresses.p_id              = NULL;
    p_instance->router_ip_addresses.val.p_string      = m_router_ip_addresses;
    p_instance->router_ip_addresses.max_len           = LWM2M_CONNECTIVITY_MONITORING_MAX_IP_ADDRESSES;

    p_instance->apn.type                              = LWM2M_LIST_TYPE_STRING;
    p_instance->apn.p_id                              = NULL;
    p_instance->apn.val.p_string                      = m_apn;
    p_instance->apn.max_len                           = LWM2M_CONNECTIVITY_MONITORING_MAX_APNS;
}


void lwm2m_instance_connectivity_statistics_init(lwm2m_connectivity_statistics_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_connectivity_statistics_t);

    p_instance->proto.object_id     = LWM2M_OBJ_CONN_STAT;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_connectivity_statistics_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT;
    p_instance->operations[1] = READ | WRATT;
    p_instance->operations[2] = READ | WRATT;
    p_instance->operations[3] = READ | WRATT;
    p_instance->operations[4] = READ | WRATT;
    p_instance->operations[5] = READ | WRATT;
    p_instance->operations[6] = EXEC;
    p_instance->operations[7] = EXEC;
    p_instance->operations[8] = READ | WRATT | WRITE;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_CONN_STAT_SMS_TX_COUNTER;
    p_instance->resource_ids[1] = LWM2M_CONN_STAT_SMS_RX_COUNTER;
    p_instance->resource_ids[2] = LWM2M_CONN_STAT_TX_DATA;
    p_instance->resource_ids[3] = LWM2M_CONN_STAT_RX_DATA;
    p_instance->resource_ids[4] = LWM2M_CONN_STAT_MAX_MSG_SIZE;
    p_instance->resource_ids[5] = LWM2M_CONN_STAT_AVG_MSG_SIZE;
    p_instance->resource_ids[6] = LWM2M_CONN_STAT_START;
    p_instance->resource_ids[7] = LWM2M_CONN_STAT_STOP;
    p_instance->resource_ids[8] = LWM2M_CONN_STAT_COLLECTION_PERIOD;
}


void lwm2m_instance_device_init(lwm2m_device_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_device_t);

    p_instance->proto.object_id     = LWM2M_OBJ_DEVICE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_device_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = READ | WRATT | DISC;
    p_instance->operations[1]  = READ | WRATT | DISC;
    p_instance->operations[2]  = READ | WRATT | DISC;
    p_instance->operations[3]  = READ | WRATT | DISC;
    p_instance->operations[4]  = EXEC;
    p_instance->operations[5]  = EXEC;
    p_instance->operations[6]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[7]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[8]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[9]  = READ | WRATT | DISC | OBSV;
    p_instance->operations[10] = READ | WRATT | DISC;
    p_instance->operations[11] = READ | WRATT | DISC | OBSV;
    p_instance->operations[12] = EXEC;
    p_instance->operations[13] = READ | WRATT | WRITE | DISC;
    p_instance->operations[14] = READ | WRATT | WRITE | DISC;
    p_instance->operations[15] = READ | WRATT | WRITE | DISC;
    p_instance->operations[16] = READ | WRATT | DISC;
    p_instance->operations[17] = READ | WRATT | DISC | OBSV;
    p_instance->operations[18] = READ | WRATT | DISC | OBSV;
    p_instance->operations[19] = READ | WRATT | DISC | OBSV;
    p_instance->operations[20] = READ | WRATT | DISC | OBSV;
    p_instance->operations[21] = READ | WRATT | DISC | OBSV;
    p_instance->operations[22] = READ | DISC;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_DEVICE_MANUFACTURER;
    p_instance->resource_ids[1]  = LWM2M_DEVICE_MODEL_NUMBER;
    p_instance->resource_ids[2]  = LWM2M_DEVICE_SERIAL_NUMBER;
    p_instance->resource_ids[3]  = LWM2M_DEVICE_FIRMWARE_VERSION;
    p_instance->resource_ids[4]  = LWM2M_DEVICE_REBOOT;
    p_instance->resource_ids[5]  = LWM2M_DEVICE_FACTORY_RESET;
    p_instance->resource_ids[6]  = LWM2M_DEVICE_AVAILABLE_POWER_SOURCES;
    p_instance->resource_ids[7]  = LWM2M_DEVICE_POWER_SOURCE_VOLTAGE;
    p_instance->resource_ids[8]  = LWM2M_DEVICE_POWER_SOURCE_CURRENT;
    p_instance->resource_ids[9]  = LWM2M_DEVICE_BATTERY_LEVEL;
    p_instance->resource_ids[10] = LWM2M_DEVICE_MEMORY_FREE;
    p_instance->resource_ids[11] = LWM2M_DEVICE_ERROR_CODE;
    p_instance->resource_ids[12] = LWM2M_DEVICE_RESET_ERROR_CODE;
    p_instance->resource_ids[13] = LWM2M_DEVICE_CURRENT_TIME;
    p_instance->resource_ids[14] = LWM2M_DEVICE_UTC_OFFSET;
    p_instance->resource_ids[15] = LWM2M_DEVICE_TIMEZONE;
    p_instance->resource_ids[16] = LWM2M_DEVICE_SUPPORTED_BINDINGS;
    p_instance->resource_ids[17] = LWM2M_DEVICE_DEVICE_TYPE;
    p_instance->resource_ids[18] = LWM2M_DEVICE_HARDWARE_VERSION;
    p_instance->resource_ids[19] = LWM2M_DEVICE_SOFTWARE_VERSION;
    p_instance->resource_ids[20] = LWM2M_DEVICE_BATTERY_STATUS;
    p_instance->resource_ids[21] = LWM2M_DEVICE_MEMORY_TOTAL;
    p_instance->resource_ids[22] = LWM2M_DEVICE_EXT_DEV_INFO;

    // Setup lists.
    p_instance->avail_power_sources.type          = LWM2M_LIST_TYPE_UINT8;
    p_instance->avail_power_sources.p_id          = NULL;
    p_instance->avail_power_sources.val.p_uint8   = m_avail_power_sources;
    p_instance->avail_power_sources.max_len       = LWM2M_DEVICE_MAX_POWER_SOURCES;

    p_instance->power_source_voltage.type         = LWM2M_LIST_TYPE_INT32;
    p_instance->power_source_voltage.p_id         = NULL;
    p_instance->power_source_voltage.val.p_int32  = m_power_source_voltage;
    p_instance->power_source_voltage.max_len      = LWM2M_DEVICE_MAX_POWER_SOURCES;

    p_instance->power_source_current.type         = LWM2M_LIST_TYPE_INT32;
    p_instance->power_source_current.p_id         = NULL;
    p_instance->power_source_current.val.p_int32  = m_power_source_current;
    p_instance->power_source_current.max_len      = LWM2M_DEVICE_MAX_POWER_SOURCES;

    p_instance->ext_dev_info.type                 = LWM2M_LIST_TYPE_INT32;
    p_instance->ext_dev_info.p_id                 = NULL;
    p_instance->ext_dev_info.val.p_int32          = m_ext_dev_info;
    p_instance->ext_dev_info.len                  = 0;
    p_instance->ext_dev_info.max_len              = LWM2M_DEVICE_MAX_DEV_INFO;

    p_instance->error_code.type                   = LWM2M_LIST_TYPE_INT32;
    p_instance->error_code.p_id                   = NULL;
    p_instance->error_code.val.p_int32            = m_error_codes;
    p_instance->error_code.max_len                = LWM2M_DEVICE_MAX_ERROR_CODES;
}


void lwm2m_instance_location_init(lwm2m_location_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_location_t);

    p_instance->proto.object_id     = LWM2M_OBJ_LOCATION;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_location_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT;
    p_instance->operations[1] = READ | WRATT;
    p_instance->operations[2] = READ | WRATT;
    p_instance->operations[3] = READ | WRATT;
    p_instance->operations[4] = READ | WRATT;
    p_instance->operations[5] = READ | WRATT;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_LOCATION_ALTITUDE;
    p_instance->resource_ids[1] = LWM2M_LOCATION_LONGITUDE;
    p_instance->resource_ids[2] = LWM2M_LOCATION_ALTITUDE;
    p_instance->resource_ids[3] = LWM2M_LOCATION_UNCERTAINTY;
    p_instance->resource_ids[4] = LWM2M_LOCATION_VELOCITY;
    p_instance->resource_ids[5] = LWM2M_LOCATION_TIMESTAMP;
}


void lwm2m_instance_software_update_init(lwm2m_software_update_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_software_update_t);

    p_instance->proto.object_id     = LWM2M_OBJ_SOFTWARE_UPDATE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_software_update_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT;
    p_instance->operations[1] = READ | WRATT;
    p_instance->operations[2] = WRITE;
    p_instance->operations[3] = WRITE;
    p_instance->operations[4] = EXEC;
    p_instance->operations[5] = READ | WRATT;
    p_instance->operations[6] = EXEC;
    p_instance->operations[7] = READ | WRATT;
    p_instance->operations[8] = READ | WRATT | WRITE;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_SW_UPDATE_PKG_NAME;
    p_instance->resource_ids[1] = LWM2M_SW_UPDATE_PKG_VERSION;
    p_instance->resource_ids[2] = LWM2M_SW_UPDATE_PACKAGE;
    p_instance->resource_ids[3] = LWM2M_SW_UPDATE_PACKAGE_URI;
    p_instance->resource_ids[4] = LWM2M_SW_UPDATE_INSTALL;
    p_instance->resource_ids[5] = LWM2M_SW_UPDATE_CHECKPOINT;
    p_instance->resource_ids[6] = LWM2M_SW_UPDATE_UNINSTALL;
    p_instance->resource_ids[7] = LWM2M_SW_UPDATE_UPDATE_STATE;
    p_instance->resource_ids[8] = LWM2M_SW_UPDATE_SUPPORTED_OBJECTS;
}

void lwm2m_instance_apn_connection_profile_init(lwm2m_apn_conn_prof_t * p_instance, uint16_t instance_id)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_apn_conn_prof_t);

    p_instance->proto.object_id     = LWM2M_OBJ_APN_CONNECTION_PROFILE;
    p_instance->proto.instance_id   = instance_id;
    p_instance->proto.num_resources = sizeof(((lwm2m_apn_conn_prof_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT | WRITE;
    p_instance->operations[1] = READ | WRATT | WRITE;
    p_instance->operations[2] = READ | WRATT | WRITE;
    p_instance->operations[3] = NONE;
    p_instance->operations[4] = READ | WRATT;
    p_instance->operations[5] = READ | WRATT;
    p_instance->operations[6] = READ | WRATT;
    p_instance->operations[7] = READ | WRATT;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_APN_CONN_PROF_PROFILE_NAME;
    p_instance->resource_ids[1] = LWM2M_APN_CONN_PROF_APN;
    p_instance->resource_ids[2] = LWM2M_APN_CONN_PROF_ENABLE_STATUS;
    p_instance->resource_ids[3] = LWM2M_APN_CONN_PROF_AUTH_TYPE;
    p_instance->resource_ids[4] = LWM2M_APN_CONN_PROF_CONN_EST_TIME;
    p_instance->resource_ids[5] = LWM2M_APN_CONN_PROF_CONN_EST_RESULT;
    p_instance->resource_ids[6] = LWM2M_APN_CONN_PROF_CONN_EST_REJECT_CAUSE;
    p_instance->resource_ids[7] = LWM2M_APN_CONN_PROF_CONN_END_TIME;

    // Setup lists.
    p_instance->conn_est_time.type                = LWM2M_LIST_TYPE_INT32;
    p_instance->conn_est_time.p_id                = NULL;
    p_instance->conn_est_time.val.p_int32         = m_conn_est_time[instance_id];
    p_instance->conn_est_time.max_len             = LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS;

    p_instance->conn_est_result.type              = LWM2M_LIST_TYPE_UINT8;
    p_instance->conn_est_result.p_id              = NULL;
    p_instance->conn_est_result.val.p_uint8       = m_conn_est_result[instance_id];
    p_instance->conn_est_result.max_len           = LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS;

    p_instance->conn_est_reject_cause.type        = LWM2M_LIST_TYPE_UINT8;
    p_instance->conn_est_reject_cause.p_id        = NULL;
    p_instance->conn_est_reject_cause.val.p_uint8 = m_conn_est_reject_cause[instance_id];
    p_instance->conn_est_reject_cause.max_len     = LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS;

    p_instance->conn_end_time.type                = LWM2M_LIST_TYPE_INT32;
    p_instance->conn_end_time.p_id                = NULL;
    p_instance->conn_end_time.val.p_int32         = m_conn_end_time[instance_id];
    p_instance->conn_end_time.max_len             = LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS;
}

void lwm2m_instance_portfolio_init(lwm2m_portfolio_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_portfolio_t);

    p_instance->proto.object_id     = LWM2M_OBJ_PORTFOLIO;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_portfolio_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT | WRITE | DISC | OBSV;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_PORTFOLIO_IDENTITY;

    // Setup lists.
    p_instance->identity.type                = LWM2M_LIST_TYPE_STRING;
    p_instance->identity.p_id                = NULL;
    p_instance->identity.val.p_string        = NULL;
    p_instance->identity.max_len             = LWM2M_PORTFOLIO_IDENTITY_INSTANCES;
}

void lwm2m_instance_connectivity_extension_init(lwm2m_connectivity_extension_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_connectivity_extension_t);

    p_instance->proto.object_id     = LWM2M_OBJ_CONN_EXT;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_connectivity_extension_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = READ | WRATT;
    p_instance->operations[1] = READ | WRATT;
    p_instance->operations[2] = READ | WRATT | WRITE;
    p_instance->operations[3] = READ | WRATT | WRITE;
    p_instance->operations[4] = READ | WRATT | WRITE;
    p_instance->operations[5] = READ | WRATT | WRITE;
    p_instance->operations[6] = READ | WRATT;
    p_instance->operations[7] = READ | WRATT;
    p_instance->operations[8] = READ | WRATT;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_CONN_EXT_ICCID;
    p_instance->resource_ids[1] = LWM2M_CONN_EXT_IMSI;
    p_instance->resource_ids[2] = LWM2M_CONN_EXT_MSISDN;
    p_instance->resource_ids[3] = LWM2M_CONN_EXT_APN_RETRIES;
    p_instance->resource_ids[4] = LWM2M_CONN_EXT_APN_RETRY_PERIOD;
    p_instance->resource_ids[5] = LWM2M_CONN_EXT_APN_RETRY_BACK_OFF_PERIOD;
    p_instance->resource_ids[6] = LWM2M_CONN_EXT_SINR;
    p_instance->resource_ids[7] = LWM2M_CONN_EXT_SRXLEV;
    p_instance->resource_ids[8] = LWM2M_CONN_EXT_CE_MODE;

    // Setup lists.
    p_instance->apn_retries.type                      = LWM2M_LIST_TYPE_UINT8;
    p_instance->apn_retries.p_id                      = NULL;
    p_instance->apn_retries.val.p_uint8               = m_apn_retries;
    p_instance->apn_retries.max_len                   = LWM2M_MAX_APN_COUNT;

    p_instance->apn_retry_period.type                 = LWM2M_LIST_TYPE_INT32;
    p_instance->apn_retry_period.p_id                 = NULL;
    p_instance->apn_retry_period.val.p_int32          = m_apn_retry_period;
    p_instance->apn_retry_period.max_len              = LWM2M_MAX_APN_COUNT;

    p_instance->apn_retry_back_off_period.type        = LWM2M_LIST_TYPE_INT32;
    p_instance->apn_retry_back_off_period.p_id        = NULL;
    p_instance->apn_retry_back_off_period.val.p_int32 = m_apn_retry_back_off_period;
    p_instance->apn_retry_back_off_period.max_len     = LWM2M_MAX_APN_COUNT;
}
