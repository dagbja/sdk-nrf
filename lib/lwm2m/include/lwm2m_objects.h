/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file lwm2m_objects.h
 *
 * @defgroup iot_sdk_lwm2m_objects OMA LWM2M objects definitions and types
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief OMA LWM2M objects definitions and types.
 *
 * @note The definitions used in this module are from the OMA LWM2M
 *       "Lightweight Machine to Machine Technical Specification - OMA_TS-LightweightM2M-V1_0-20131210-C".
 *       The specification could be found at http://openmobilealliance.org/.
 */

#ifndef LWM2M_OBJECTS_H__
#define LWM2M_OBJECTS_H__

#include <stdint.h>
#include <stdbool.h>
#include <lwm2m_api.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @brief LWM2M Enabler Object IDs Appendix E  */
#define LWM2M_OBJ_SECURITY                    0
#define LWM2M_OBJ_SERVER                      1
#define LWM2M_OBJ_ACCESS_CONTROL              2
#define LWM2M_OBJ_DEVICE                      3
#define LWM2M_OBJ_CONN_MON                    4
#define LWM2M_OBJ_FIRMWARE                    5
#define LWM2M_OBJ_LOCATION                    6
#define LWM2M_OBJ_CONN_STAT                   7

/* @brief LWM2M Registry Objects */
#define LWM2M_OBJ_SOFTWARE_UPDATE             9
#define LWM2M_OBJ_APN_CONNECTION_PROFILE      11
#define LWM2M_OBJ_PORTFOLIO                   16

/* @brief AT&T Connectivity Extension Object */
#define LWM2M_OBJ_CONN_EXT                    10308

/* LWM2M Security Resource IDs Appendix E.1 */
#define LWM2M_SECURITY_SERVER_URI             0
#define LWM2M_SECURITY_BOOTSTRAP_SERVER       1
#define LWM2M_SECURITY_SECURITY_MODE          2
#define LWM2M_SECURITY_PUBLIC_KEY             3
#define LWM2M_SECURITY_SERVER_PUBLIC_KEY      4
#define LWM2M_SECURITY_SECRET_KEY             5
#define LWM2M_SECURITY_SMS_SECURITY_MODE      6
#define LWM2M_SECURITY_SMS_BINDING_KEY_PARAM  7
#define LWM2M_SECURITY_SMS_BINDING_SECRET_KEY 8
#define LWM2M_SECURITY_SERVER_SMS_NUMBER      9
#define LWM2M_SECURITY_SHORT_SERVER_ID        10
#define LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME   11


/* LWM2M Server Resources Appendix E.2 */
#define LWM2M_SERVER_SHORT_SERVER_ID             0
#define LWM2M_SERVER_LIFETIME                    1
#define LWM2M_SERVER_DEFAULT_MIN_PERIOD          2
#define LWM2M_SERVER_DEFAULT_MAX_PERIOD          3
#define LWM2M_SERVER_DISABLE                     4
#define LWM2M_SERVER_DISABLE_TIMEOUT             5
#define LWM2M_SERVER_NOTIFY_WHEN_DISABLED        6
#define LWM2M_SERVER_BINDING                     7
#define LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER 8
#define LWM2M_SERVER_BOOTSTRAP_REQUEST_TRIGGER   9


/* LWM2M Firmware update Resources Appendix E.6 */
#define LWM2M_FIRMWARE_PACKAGE                          0
#define LWM2M_FIRMWARE_PACKAGE_URI                      1
#define LWM2M_FIRMWARE_UPDATE                           2
#define LWM2M_FIRMWARE_STATE                            3
#define LWM2M_FIRMWARE_LEGACY_DO_NOT_RENDER             4
#define LWM2M_FIRMWARE_UPDATE_RESULT                    5
#define LWM2M_FIRMWARE_PKG_NAME                         6
#define LWM2M_FIRMWARE_PKG_VERSION                      7
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT 8
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD  9

#define LWM2M_FIRMWARE_STATE_IDLE                      0
#define LWM2M_FIRMWARE_STATE_DOWNLOADING               1
#define LWM2M_FIRMWARE_STATE_DOWNLOADED                2
#define LWM2M_FIRMWARE_STATE_UPDATING                  3

/* Default value, to be set at the beginning of the DFU process. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_DEFAULT                      0
/* Firmware updated successfully */
#define LWM2M_FIRMWARE_UPDATE_RESULT_SUCCESS                      1
/* Not enough flash memory. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_STORAGE                2
/* Out of RAM during downloading process */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_MEMORY                 3
/*  Connection lost during downloading process. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CONN_LOST              4
/*  Integrity check failure for new downloaded package. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_CRC                    5
/* Unsupported package type. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PKG_TYPE   6
/* Invalid URI. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_INVALID_URI            7
/* Firmware update failed. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_FIRMWARE_UPDATE_FAILED 8
/* Unsupported protocol. */
#define LWM2M_FIRMWARE_UPDATE_RESULT_ERROR_UNSUPPORTED_PROTOCOL   9

#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_COAP  0
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_COAPS 1
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_HTTP  2
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_PROTOCOL_SUPPORT_HTTPS 3

#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD_PULL_ONLY     0
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD_PUSH_ONLY     1
#define LWM2M_FIRMWARE_FIRMWARE_UPDATE_DELIVERY_METHOD_PUSH_AND_PULL 2



/* LWM2M Access Control Resources */
#define LWM2M_ACCESS_CONTROL_OBJECT_ID          0
#define LWM2M_ACCESS_CONTROL_INSTANCE_ID        1
#define LWM2M_ACCESS_CONTROL_ACL                2
#define LWM2M_ACCESS_CONTROL_CONTROL_OWNER      3

/* One Access Control instance per any object instance registered in the
   request handler, excluding Security instances and the Bootstrap Server
   instance. */
#define LWM2M_ACCESS_CONTROL_MAX_INSTANCES      14

/* LWM2M Connectivity Monitoring Resources */
#define LWM2M_CONN_MON_NETWORK_BEARER           0
#define LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER 1
#define LWM2M_CONN_MON_RADIO_SIGNAL_STRENGTH    2
#define LWM2M_CONN_MON_LINK_QUALITY             3
#define LWM2M_CONN_MON_IP_ADDRESSES             4
#define LWM2M_CONN_MON_ROUTER_IP_ADRESSES       5
#define LWM2M_CONN_MON_LINK_UTILIZATION         6
#define LWM2M_CONN_MON_APN                      7
#define LWM2M_CONN_MON_CELL_ID                  8
#define LWM2M_CONN_MON_SMNC                     9
#define LWM2M_CONN_MON_SMCC                     10

/* LWM2M Connectivity Statistics */
#define LWM2M_CONN_STAT_SMS_TX_COUNTER          0
#define LWM2M_CONN_STAT_SMS_RX_COUNTER          1
#define LWM2M_CONN_STAT_TX_DATA                 2
#define LWM2M_CONN_STAT_RX_DATA                 3
#define LWM2M_CONN_STAT_MAX_MSG_SIZE            4
#define LWM2M_CONN_STAT_AVG_MSG_SIZE            5
#define LWM2M_CONN_STAT_START                   6
#define LWM2M_CONN_STAT_STOP                    7
#define LWM2M_CONN_STAT_COLLECTION_PERIOD       8

/* LWM2M Device */
#define LWM2M_DEVICE_MANUFACTURER               0
#define LWM2M_DEVICE_MODEL_NUMBER               1
#define LWM2M_DEVICE_SERIAL_NUMBER              2
#define LWM2M_DEVICE_FIRMWARE_VERSION           3
#define LWM2M_DEVICE_REBOOT                     4
#define LWM2M_DEVICE_FACTORY_RESET              5
#define LWM2M_DEVICE_AVAILABLE_POWER_SOURCES    6
#define LWM2M_DEVICE_POWER_SOURCE_VOLTAGE       7
#define LWM2M_DEVICE_POWER_SOURCE_CURRENT       8
#define LWM2M_DEVICE_BATTERY_LEVEL              9
#define LWM2M_DEVICE_MEMORY_FREE                10
#define LWM2M_DEVICE_ERROR_CODE                 11
#define LWM2M_DEVICE_RESET_ERROR_CODE           12
#define LWM2M_DEVICE_CURRENT_TIME               13
#define LWM2M_DEVICE_UTC_OFFSET                 14
#define LWM2M_DEVICE_TIMEZONE                   15
#define LWM2M_DEVICE_SUPPORTED_BINDINGS         16
#define LWM2M_DEVICE_DEVICE_TYPE                17
#define LWM2M_DEVICE_HARDWARE_VERSION           18
#define LWM2M_DEVICE_SOFTWARE_VERSION           19
#define LWM2M_DEVICE_BATTERY_STATUS             20
#define LWM2M_DEVICE_MEMORY_TOTAL               21
#define LWM2M_DEVICE_EXT_DEV_INFO               22

#define LWM2M_DEVICE_MAX_POWER_SOURCES          8
#define LWM2M_DEVICE_MAX_ERROR_CODES            8
#define LWM2M_DEVICE_MAX_DEV_INFO               1

/* LWM2M Location */
#define LWM2M_LOCATION_LATITUDE                 0
#define LWM2M_LOCATION_LONGITUDE                1
#define LWM2M_LOCATION_ALTITUDE                 2
#define LWM2M_LOCATION_UNCERTAINTY              3
#define LWM2M_LOCATION_VELOCITY                 4
#define LWM2M_LOCATION_TIMESTAMP                5

/* LWM2M Software update */
#define LWM2M_SW_UPDATE_PKG_NAME                0
#define LWM2M_SW_UPDATE_PKG_VERSION             1
#define LWM2M_SW_UPDATE_PACKAGE                 2
#define LWM2M_SW_UPDATE_PACKAGE_URI             3
#define LWM2M_SW_UPDATE_INSTALL                 4
#define LWM2M_SW_UPDATE_CHECKPOINT              5
#define LWM2M_SW_UPDATE_UNINSTALL               6
#define LWM2M_SW_UPDATE_UPDATE_STATE            7
#define LWM2M_SW_UPDATE_SUPPORTED_OBJECTS       8

/* LWM2M APN Connection Profile */
#define LWM2M_APN_CONN_PROF_PROFILE_NAME          0
#define LWM2M_APN_CONN_PROF_APN                   1
#define LWM2M_APN_CONN_PROF_ENABLE_STATUS         3
#define LWM2M_APN_CONN_PROF_AUTH_TYPE             4
#define LWM2M_APN_CONN_PROF_CONN_EST_TIME         9
#define LWM2M_APN_CONN_PROF_CONN_EST_RESULT       10
#define LWM2M_APN_CONN_PROF_CONN_EST_REJECT_CAUSE 11
#define LWM2M_APN_CONN_PROF_CONN_END_TIME         12

#define LWM2M_APN_CONN_PROF_MAX_TIMESTAMPS        5

/* LWM2M Portfolio */
#define LWM2M_PORTFOLIO_IDENTITY                0

#define LWM2M_PORTFOLIO_HOST_DEVICE_ID           0
#define LWM2M_PORTFOLIO_HOST_DEVICE_MANUFACTURER 1
#define LWM2M_PORTFOLIO_HOST_DEVICE_MODEL        2
#define LWM2M_PORTFOLIO_HOST_DEVICE_SW_VERSION   3
#define LWM2M_PORTFOLIO_IDENTITY_INSTANCES       4

#define LWM2M_PORTFOLIO_MAX_INSTANCES            4

/* AT&T Connectivity extension */
#define LWM2M_CONN_EXT_ICCID                     1
#define LWM2M_CONN_EXT_IMSI                      2
#define LWM2M_CONN_EXT_MSISDN                    3
#define LWM2M_CONN_EXT_APN_RETRIES               4
#define LWM2M_CONN_EXT_APN_RETRY_PERIOD          5
#define LWM2M_CONN_EXT_APN_RETRY_BACK_OFF_PERIOD 6
#define LWM2M_CONN_EXT_SINR                      7
#define LWM2M_CONN_EXT_SRXLEV                    8
#define LWM2M_CONN_EXT_CE_MODE                   9


/**
 * LWM2M Enabler
 */

typedef struct
{
    lwm2m_instance_t           proto;            /* Internal. MUST be first. */
    uint8_t                    operations[12];   /* Internal. MUST be second. */
    uint16_t                   resource_ids[12]; /* Internal. MUST be third. */

    /* Public members. */
    lwm2m_string_t             server_uri;                   // 0-255 bytes
    bool                       bootstrap_server;
    uint8_t                    security_mode;                // 0-4
    lwm2m_opaque_t             public_key;
    lwm2m_opaque_t             server_public_key;
    lwm2m_opaque_t             secret_key;
    uint8_t                    sms_security_mode;            // 0-255
    lwm2m_opaque_t             sms_binding_key_param;        // 6 bytes
    lwm2m_opaque_t             sms_binding_secret_keys;      // 16-32-48 bytes
    lwm2m_string_t             sms_number;
    uint16_t                   short_server_id;              // 1-65534
    lwm2m_time_t               client_hold_off_time;

} lwm2m_security_t;

typedef struct
{
    lwm2m_instance_t           proto;            /* Internal. MUST be first. */
    uint8_t                    operations[10];   /* Internal. MUST be second. */
    uint16_t                   resource_ids[10]; /* Internal. MUST be third. */

    /* Public members. */
    uint16_t                   short_server_id;              // 1-65535
    lwm2m_time_t               lifetime;
    lwm2m_time_t               default_minimum_period;
    lwm2m_time_t               default_maximum_period;
    void *                     disable;                      // Function pointer.
    lwm2m_time_t               disable_timeout;
    bool                       notification_storing_on_disabled;
    lwm2m_string_t             binding;
    void *                     registration_update_trigger;  // Function pointer.
    void *                     bootstrap_request_trigger;    // Function pointer.

} lwm2m_server_t;

typedef struct
{
    lwm2m_instance_t           proto;            /* Internal. MUST be first. */
    uint8_t                    operations[4];    /* Internal. MUST be second. */
    uint16_t                   resource_ids[4];  /* Internal. MUST be third. */

    /* Public members. */
    uint16_t                   object_id;
    uint16_t                   instance_id;
    lwm2m_list_t               acl;
    uint16_t                   control_owner;

} lwm2m_access_control_t;

typedef struct
{
    lwm2m_instance_t           proto;            /* Internal. MUST be first. */
    uint8_t                    operations[10];   /* Internal. MUST be second. */
    uint16_t                   resource_ids[10]; /* Internal. MUST be third. */

    /* Public members. */
    lwm2m_opaque_t             package;
    lwm2m_string_t             package_uri;
    uint8_t                    state;
    uint8_t                    update_result;
    lwm2m_string_t             pkg_name;
    lwm2m_string_t             pkg_version;
    lwm2m_list_t               firmware_update_protocol_support;
    uint8_t                    firmware_update_delivery_method;

} lwm2m_firmware_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[11];
    uint16_t                   resource_ids[11];

    /* Public members. */
    int32_t                    network_bearer;
    lwm2m_list_t               available_network_bearer;
    int32_t                    radio_signal_strength;    // Unit: dBm
    int32_t                    link_quality;
    lwm2m_list_t               ip_addresses;
    lwm2m_list_t               router_ip_addresses;
    uint8_t                    link_utilization;         // Unit: percent
    lwm2m_list_t               apn;
    int32_t                    cell_id;
    int32_t                    smnc;
    int32_t                    smcc;

} lwm2m_connectivity_monitoring_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[9];
    uint16_t                   resource_ids[9];

    /* Public members. */
    uint32_t                   sms_tx_counter;
    uint32_t                   sms_rx_counter;
    uint32_t                   tx_data;              // Unit: kilo-bytes
    uint32_t                   rx_data;              // Unit: kilo-bytes
    uint32_t                   max_message_size;     // Unit: byte
    uint32_t                   average_message_size; // Unit: byte
    /* Start is Execute only */
    /* Stop is Execute only */
    uint32_t                   collection_period;    // Unit: seconds

} lwm2m_connectivity_statistics_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[23];
    uint16_t                   resource_ids[23];

    /* Public members. */
    lwm2m_string_t             manufacturer;
    lwm2m_string_t             model_number;
    lwm2m_string_t             serial_number;
    lwm2m_string_t             firmware_version;
    /* Reboot is execute only */
    /* Factory reset is execute only */
    lwm2m_list_t               avail_power_sources;  // Range: 0-7
    lwm2m_list_t               power_source_voltage; // Unit: mV
    lwm2m_list_t               power_source_current; // Unit: mA
    uint8_t                    battery_level;        // Unit: percent
    int32_t                    memory_free;          // Unit: KB
    lwm2m_list_t               error_code;
    /* Reset Error code is execute only */
    lwm2m_time_t               current_time;
    lwm2m_string_t             utc_offset;
    lwm2m_string_t             timezone;
    lwm2m_string_t             supported_bindings;
    lwm2m_string_t             device_type;
    lwm2m_string_t             hardware_version;
    lwm2m_string_t             software_version;
    int32_t                    battery_status;
    int32_t                    memory_total;
    lwm2m_list_t               ext_dev_info;

} lwm2m_device_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[6];
    uint16_t                   resource_ids[6];

    /* Public members. */
    lwm2m_string_t             latitude;    // Unit: Deg
    lwm2m_string_t             longitude;   // Unit: Deg
    lwm2m_string_t             altitude;    // Unit: m
    lwm2m_string_t             uncertainty; // Unit: m
    lwm2m_opaque_t             velocity;    // Unit: Refers to 3GPP GAD specs
    lwm2m_time_t               timestamp;   // Range: 0-6

} lwm2m_location_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[9];
    uint16_t                   resource_ids[9];

    /* Public members. */
    lwm2m_string_t             pkg_name;
    lwm2m_string_t             pkg_version;
    lwm2m_opaque_t             package;
    lwm2m_string_t             package_uri;
    /* Install is execute only */
    uint16_t                   checkpoint;  // TODO: this is of type Objlnk
    /* Uninstall is execute only */
    uint8_t                    update_state; // Range: 1-5
    bool                       update_supported_objects;

} lwm2m_software_update_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[8];
    uint16_t                   resource_ids[8];

    /* Public members. */
    lwm2m_string_t             profile_name;
    lwm2m_string_t             apn;
    bool                       enable_status;
    uint8_t                    authentication_type;   // 0: PAP; 1: CHAP
    lwm2m_list_t               conn_est_time;         // UTC Time of connection request
    lwm2m_list_t               conn_est_result;       // 0: accepted; 1: rejected
    lwm2m_list_t               conn_est_reject_cause; // 3GPP TS 24.008
    lwm2m_list_t               conn_end_time;         // UTC Time of connection end

} lwm2m_apn_conn_prof_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[1];
    uint16_t                   resource_ids[1];

    /* Public members. */
    lwm2m_list_t               identity;

} lwm2m_portfolio_t;

typedef struct
{
    lwm2m_instance_t           proto;
    uint8_t                    operations[9];
    uint16_t                   resource_ids[9];

    /* Public members. */
    lwm2m_string_t             iccid;
    lwm2m_string_t             imsi;
    lwm2m_string_t             msisdn;
    lwm2m_list_t               apn_retries;
    lwm2m_list_t               apn_retry_period; // Unit: s
    lwm2m_list_t               apn_retry_back_off_period; // Unit: s
    int32_t                    sinr;
    int32_t                    srxlev;
    lwm2m_string_t             ce_mode;

} lwm2m_connectivity_extension_t;


/**@brief Allocate lwm2m_string_t memory to hold a string.
 *
 * @param[in]  p_payload Buffer which holds a string.
 * @param[in]  payload_len  Length of the value in the buffer.
 * @param[out] p_string By reference pointer to the result lwm2m_string_t.
 *
 * @return NRF_SUCCESS       If allocation was successful
 * @retval NRF_ERROR_NO_MEM  If allocation was unsuccessful
 */
uint32_t lwm2m_bytebuffer_to_string(const char * p_payload, uint16_t payload_len, lwm2m_string_t * p_string);

/**@brief Allocate lwm2m_opaque_t memory to hold a opaque.
 *
 * @param[in]  p_payload Buffer which holds a opaque.
 * @param[in]  payload_len  Length of the value in the buffer.
 * @param[out] p_opaque By reference pointer to the result lwm2m_opaque_t.
 *
 * @return NRF_SUCCESS       If allocation was successful
 * @retval NRF_ERROR_NO_MEM  If allocation was unsuccessful
 */
uint32_t lwm2m_bytebuffer_to_opaque(const char * p_payload, uint16_t payload_len, lwm2m_opaque_t * p_opaque);

/**@brief Allocate lwm2m_list_t memory to hold a list.
 *
 * @param[in]  p_payload Buffer which holds a list.
 * @param[in]  payload_len  Length of the value in the buffer.
 * @param[out] p_list By reference pointer to the result lwm2m_list_t.
 *
 * @return NRF_SUCCESS       If allocation was successful
 * @retval NRF_ERROR_NO_MEM  If allocation was unsuccessful
 */
uint32_t lwm2m_bytebuffer_to_list(const char * p_payload, uint16_t payload_len, lwm2m_list_t * p_list);

/**@brief Free allocated memory in lwm2m_string_t.
 *
 * @param[in] p_string By reference pointer to the lwm2m_string_t.
 *
 * @return NRF_SUCCESS  If deallocation was successful
 */
uint32_t lwm2m_string_free(lwm2m_string_t * p_string);

/**@brief Free allocated memory in lwm2m_opaque_t.
 *
 * @param[in] p_opaque By reference pointer to the lwm2m_opaque_t.
 *
 * @return NRF_SUCCESS  If deallocation was successful
 */
uint32_t lwm2m_opaque_free(lwm2m_opaque_t * p_opaque);

/**@brief Initialize a LWM2M security object instance.
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_security_init(lwm2m_security_t * p_instance);

/**@brief Initialize a LWM2M server object instance.
 *
 * @details Must be called before any use of the instance.

 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_server_init(lwm2m_server_t * p_instance);

/**@brief Initialize a LWM2M access control object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_access_control_init(lwm2m_access_control_t * p_instance, uint16_t instance_id);

/**@brief Initialize a LWM2M firmware object instance.
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_firmware_init(lwm2m_firmware_t * p_instance);

/**@brief Initialize a LWM2M connectivity monitoring object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_connectivity_monitoring_init(lwm2m_connectivity_monitoring_t * p_instance);

/**@brief Initialize a LWM2M connectivity statistics object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_connectivity_statistics_init(lwm2m_connectivity_statistics_t * p_instance);

/**@brief Initialize a LWM2M device object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_device_init(lwm2m_device_t * p_instance);

/**@brief Initialize a LWM2M location object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_location_init(lwm2m_location_t * p_instance);

/**@brief Initialize a LWM2M software update object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_software_update_init(lwm2m_software_update_t * p_instance);

/**@brief Initialize a LWM2M APN Connection Profile object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance  Pointer to instance structure to initialize.
 * @param[in] instance_id Instance identifier.
 */
void lwm2m_instance_apn_connection_profile_init(lwm2m_apn_conn_prof_t * p_instance, uint16_t instance_id);

/**@brief Initialize a LWM2M Portfolio object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_portfolio_init(lwm2m_portfolio_t * p_instance);

/**@brief Initialize an AT&T connectivity extension object instance
 *
 * @details Must be called before any use of the instance.
 *
 * @param[in] p_instance Pointer to instance structure to initialize.
 */
void lwm2m_instance_connectivity_extension_init(lwm2m_connectivity_extension_t * p_instance);

#ifdef __cplusplus
}
#endif

#endif // LWM2M_OBJECTS_H__

/** @} */
