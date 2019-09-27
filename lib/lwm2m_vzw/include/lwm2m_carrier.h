/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CARRIER_H__

#include <stdint.h>
#include <stddef.h>

/**@file lwm2m_carrier.h
 *
 * @defgroup lwm2m_carrier_event LWM2M carrier library events
 * @{
 */
#define LWM2M_CARRIER_EVENT_BSDLIB_INIT  1  /**< BSD library initialized. */
#define LWM2M_CARRIER_EVENT_CONNECT      2  /**< LTE link connected. */
#define LWM2M_CARRIER_EVENT_DISCONNECT   3  /**< LTE link will disconnect. */
#define LWM2M_CARRIER_EVENT_BOOTSTRAPPED 4  /**< LWM2M carrier bootstrapped. */
#define LWM2M_CARRIER_EVENT_READY        5  /**< LWM2M carrier registered. */
#define LWM2M_CARRIER_EVENT_FOTA_START   6  /**< Modem update started. */
#define LWM2M_CARRIER_EVENT_REBOOT       10 /**< Application will reboot. */
/**@} */

/**
 * @defgroup lwm2m_carrier_api LWM2M carrier library API
 * @brief LWM2M carrier library API functions.
 * @{
 */
/**
 * @brief LWM2M carrier library event structure.
 */
typedef struct {
	/** Event type. */
	uint32_t type;
	/** Event data. Can be NULL, depending on event type. */
	void *data;
} lwm2m_carrier_event_t;

/**
 * @brief Structure holding LWM2M carrier library initialization parameters.
 */
typedef struct {
	/** URI of the bootstrap server, null-terminated. */
	char *bootstrap_uri;
	/** Pre-shared key that the device will use. */
	char *psk;
	/** Length of the pre-shared key. */
	size_t psk_length;
} lwm2m_carrier_config_t;

/**
 * @brief LWM2M device power sources type.
 */
typedef enum {
    DEVICE_POWER_SOURCE_DC                        = 0,
    DEVICE_POWER_SOURCE_INTERNAL_BATTERY          = 1,
    DEVICE_POWER_SOURCE_EXTERNAL_BATTERY          = 2,
    DEVICE_POWER_SOURCE_ETHERNET                  = 4,
    DEVICE_POWER_SOURCE_USB                       = 5,
    DEVICE_POWER_SOURCE_AC                        = 6,
    DEVICE_POWER_SOURCE_SOLAR                     = 7
} lwm2m_device_power_source_t;

/**
 * @brief LWM2M device error code type.
 */
typedef enum {
    DEVICE_ERROR_CODE_NO_ERROR                  = 0,
    DEVICE_ERROR_CODE_LOW_CHARGE                = 1,
    DEVICE_ERROR_CODE_EXTERNAL_SUPPLY_OFF       = 2,
    DEVICE_ERROR_CODE_GPS_FAILURE               = 3,
    DEVICE_ERROR_CODE_LOW_SIGNAL                = 4,
    DEVICE_ERROR_CODE_OUT_OF_MEMORY             = 5,
    DEVICE_ERROR_CODE_SMS_FAILURE               = 6,
    DEVICE_ERROR_CODE_IP_CONNECTIVITY_FAILURE   = 7,
    DEVICE_ERROR_CODE_PERIPHERAL_MALFUNCTION    = 8
} lwm2m_device_error_code_t;

/**
 * @brief LWM2M device battery status type.
 *
 * @note These values are only valid for the LWM2M Device INTERNAL_BATTERY if present.
 */
typedef enum {
    DEVICE_BATTERY_STATUS_NORMAL                    = 0,
    DEVICE_BATTERY_STATUS_CHARGING                  = 1,
    DEVICE_BATTERY_STATUS_CHARGE_COMPLETE           = 2,
    DEVICE_BATTERY_STATUS_DAMAGED                   = 3,
    DEVICE_BATTERY_STATUS_LOW_BATTERY               = 4,
    DEVICE_BATTERY_STATUS_NOT_INSTALLED             = 5,
    DEVICE_BATTERY_STATUS_UNKNOWN                   = 6
} lwm2m_device_battery_status_t;

/**
 * @brief Initialize the LWM2M carrier library.
 *
 * @param[in] config Configuration parameters for the library.
 *
 * @note The library does not create a copy of the config parameters, hence the
 *       application has to make sure that the parameters provided are valid
 *       throughout the application lifetime (i. e. placed in static memory
 *       or in flash).
 *
 * @return 0 on success, negative error code on error.
 */
int lwm2m_carrier_init(const lwm2m_carrier_config_t *config);

/**
 * @brief LWM2M carrier library main function.
 *
 * This is a non-return function, intended to run on a separate thread.
 */
void lwm2m_carrier_run(void);

/**
 * @brief Function to read current UTC time
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @return  Current UTC time since Epoch in seconds
 */
int32_t lwm2m_carrier_utc_time_read(void);

/**
 * @brief Function to read offset to UTC time
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @return  UTC offset in minutes
 */
int lwm2m_carrier_utc_offset_read(void);

/**
 * @brief Function to read timezone
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @return  Null-terminated timezone string pointer, IANA Timezone (TZ) database format
 */
const char* lwm2m_carrier_timezone_read(void);

/**
 * @brief Function to write current UTC time (LWM2M server write operation)
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @param[in] time Time since Epoch in seconds
 *
 * @return 0 on success, negative error code on error.
 */
int lwm2m_carrier_utc_time_write(int32_t time);

/**
 * @brief Function to write UTC offset (LWM2M server write operation)
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @param[in] offset UTC offset in minutes
 *
 * @return 0 on success, negative error code on error.
 */
int lwm2m_carrier_utc_offset_write(int offset);

/**
 * @brief Function to write timezone (LWM2M server write operation)
 *
 * @note This function can be implemented by the application, if custom time management
 *       is needed.
 *
 * @param[in] p_tz Null-terminated timezone string pointer
 *
 * @return 0 on success, negative error code on error.
 */
int lwm2m_carrier_timezone_write(const char* p_tz);

/**
 * @brief LWM2M carrier library event handler.
 *
 * This function will be called by the LWM2M carrier library whenever some event
 * significant for the application occurs.
 *
 * @note This function has to be implemented by the application.
 *
 * @param[in] event LWM2M carrier event that occurred.
 */
void lwm2m_carrier_event_handler(const lwm2m_carrier_event_t *event);
/**@} */

/**
 * @brief      Set the available power sources supported and used by the LWM2M device.
 *
 * @note       It is necessary to call this function before any other device power source related
 *             functions listed in this file, as any updates of voltage/current measurements performed
 *             on power sources that have not been reported will be discarded.
 * @note       Upon consecutive calls of this function, the corresponding current and voltage measurements
 *             will be reset to 0. Similarly, the battery status will be set to UNKNOWN and the battery
 *             level to 0%.
 *
 * @param[in]  power_sources          Array of available device power sources.
 * @param[in]  power_source_count     Number of power sources currently used by the device.
 *
 * @return     E2BIG    If the reported number of power sources is bigger than the maximum supported.
 * @return     EINVAL   If one or more of the power sources are not supported.
 * @return     0        If the available power sources have been set successfully.
 */
int lwm2m_device_avail_power_sources_set(lwm2m_device_power_source_t * power_sources, uint8_t power_source_count);

/**
 * @brief      Set or update the latest voltage measurements made on one of the available device power sources.
 *
 * @note       The voltage measurement needs to be specified in milivolts (mV) and is to be assigned to
 *             one of the available power sources.
 *
 * @param[in]  power_source           Power source to which the measurement corresponds.
 * @param[in]  value                  Voltage measurement expressed in mV.
 *
 * @return     EINVAL   If the power source is not supported.
 * @return     ENODEV   If the power source is not listed as an available power source.
 * @return     0        If the voltage measurements have been updated successfully.
 */
int lwm2m_device_power_source_voltage_set(lwm2m_device_power_source_t power_source, int32_t value);

/**
 * @brief      Set or update the latest current measurements made on one of the available device power sources.
 *
 * @note       The current measurement needs to be specified in miliampers (mA) and is to be assigned to
 *             one of the available power sources.
 *
 * @param[in]  power_source           Power source to which the measurement corresponds.
 * @param[in]  value                  Current measurement expressed in mA.
 *
 * @return     EINVAL   If the power source is not supported.
 * @return     ENODEV   If the power source is not listed as an available power source.
 * @return     0        If the current measurements have been updated successfully.
 */
int lwm2m_device_power_source_current_set(lwm2m_device_power_source_t power_source, int32_t value);

/**
 * @brief      Set or update the latest battery level (internal battery).
 *
 * @note       The battery level is to be specified as a percentage, hence values outside
 *             the range 0-100 will be ignored.
 *
 * @note       The value is only valid for the Device internal battery if present.
 *
 * @param[in]  battery_level          Internal battery level percentage to be updated.
 *
 * @return     EINVAL   If the specified battery level lies outside the 0-100% range.
 * @return     ENODEV   If internal battery is not listed as an available power source.
 * @return     0        If the battery level has been updated successfully.
 */
int lwm2m_device_battery_level_set(uint8_t battery_level);

/**
 * @brief      Set or update the latest battery status (internal battery).
 *
 * @note       The value is only valid for the Device internal battery.
 *
 * @param[in]  battery_status         Internal battery status to be reported.
 *
 * @return     EINVAL   If the battery status is not supported.
 * @return     ENODEV   If internal battery is not listed as an available power source.
 * @return     0        If the battery status has been updated successfully.
 */
int lwm2m_device_battery_status_set(lwm2m_device_battery_status_t battery_status);

/**
 * @brief      Set the LWM2M device type.
 *
 * @note       Type of the LWM2M device specified by the manufacturer.
 *
 * @param[in]  device_type     Null terminated string specifying the type of the LWM2M device.
 *
 * @return     EINVAL          If the input argument is a NULL pointer.
 * @return     E2BIG           If the input string is too long.
 * @return     ENOMEM          If it was not possible to allocate memory storage to hold the string.
 * @return     0               If the device type has been set successfully.
 */
int lwm2m_device_type_set(char * device_type);

/**
 * @brief      Set the LWM2M device software version.
 *
 * @note       High level device software version (application).
 *
 * @param[in]  software_version    Null terminated string specifying the current software version
 *                                 of the LWM2M device.
 *
 * @return     EINVAL              If the input argument is a NULL pointer.
 * @return     E2BIG               If the input string is too long.
 * @return     ENOMEM              If it was not possible to allocate memory storage to hold the string.
 * @return     0                   If the software version has been set successfully.
 */
int lwm2m_device_software_version_set(char * software_version);

/**
 * @brief      Update the device object instance error code by adding an individual error.
 *
 * @note       Upon initialisation of the device object instance, the error code is specified as 0,
 *             indicating no error. The error code is to be updated whenever a new error occurs.
 * @note       If the reported error is NO_ERROR, all existing error codes will be reset.
 * @note       If the reported error is already present, the error code will remain unchanged.
 *
 * @param[in]  error               Individual error to be added.
 *
 * @return     EINVAL              If the error code is not supported.
 * @return     0                   If the error code has been added successfully.
 */
int lwm2m_device_error_code_add(lwm2m_device_error_code_t error);

/**
 * @brief      Update the device object instance error code by removing and individual error.
 *
 * @note       Upon initialisation of the device object instance, the error code is specified as 0,
 *             indicating no error. The error code is to be updated whenever an error is no longer
 *             present. When all the errors are removed, the error code is specified as 0, hence
 *             indicating no error again.
 *
 * @param[in]  error           Individual error code to be removed.
 *
 * @return     EINVAL          If the error code is not supported.
 * @return     ENOENT          If the error to be removed is not present on the error code list.
 * @return     0               If the error has been removed successfully.
 */
int lwm2m_device_error_code_remove(lwm2m_device_error_code_t error);

/**
 * @brief      Set the total amount of storage space to store data and software in the LWM2M Device.
 *
 * @note       The value is expressed in kilobytes (kB).
 *
 * @param[in]  memory_total    Total amount of storage space in kilobytes.
 *
 * @return     EINVAL          If the reported value of total amount of storage space is a negative
 *                             value.
 * @return     0               If the total amount of storage space has been set successfully.
 */
int lwm2m_device_memory_total_set(int32_t memory_total);

/**
 * @brief      Read the estimated current available amount of storage space to store data and
 *             software in the LWM2M Device.
 *
 * @note       This function can be implemented by the application if needed.
 *
 * @return     Available amount of storage space expressed in kB.
 */
int lwm2m_device_memory_free_read(void);


#endif /* LWM2M_CARRIER_H__ */
