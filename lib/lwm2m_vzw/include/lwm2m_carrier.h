/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_CARRIER_H__

#define LWM2M_CARRIER_EVENT_BSDLIB_INIT 1 /* bsdlib library initialized. */
#define LWM2M_CARRIER_EVENT_CONNECT     2 /* LTE link connected. */
#define LWM2M_CARRIER_EVENT_DISCONNECT  3 /* LTE link will disconnect. */
#define LWM2M_CARRIER_EVENT_READY       4 /* LWM2M carrier registered. */
#define LWM2M_CARRIER_EVENT_REBOOT      5 /* Application will reboot. */

typedef struct
{
    uint32_t type; /* Event type. */
    void   * data; /* Event data, can be NULL, depending on event type. */
} lwm2m_carrier_event_t;

/* This has to be implemented by the application. */
void lwm2m_carrier_event_handler(const lwm2m_carrier_event_t * event);

typedef struct
{
    char * bootstrap_uri; /* URI of the bootstrap server. Shall be a NULL terminated string. */
    char * psk;           /* Pre-shared key that the device will use. */
    size_t psk_length;    /* Length of the pre-shared key. */
} lwm2m_carrier_config_t;

int lwm2m_carrier_init(const lwm2m_carrier_config_t * config);
void lwm2m_carrier_run(void);

/**
 * @brief Timezone function that returns a timezone string based on the offset to GMT.
 *
 * @note  It can be overwritten by the application.
 *        Defaults to Etc/GMT+-X format
 *
 * @param[in] tz_offset Offset as the number of minutes west of GMT
 * @param[in] dst       Daylight saving in minutes included in the tz_offset.
 *
 * @return  Pointer to the null terminated timezone string.
 *          In case no valid timezone found, NULL pointer can be returned.
 */
extern const char * lwm2m_carrier_timezone(int32_t tz_offset, int32_t dst);

#endif // LWM2M_CARRIER_H__
