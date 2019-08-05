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

int lwm2m_carrier_init(void);
void lwm2m_carrier_run(void);

#endif // LWM2M_CARRIER_H__
