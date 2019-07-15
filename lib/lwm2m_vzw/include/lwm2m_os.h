/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_OS_H__

#include <stdint.h>
#include <stddef.h>

int lwm2m_os_init(void);

void *lwm2m_os_malloc(size_t size);
void lwm2m_os_free(void *ptr);

int64_t lwm2m_os_uptime_get(void);
int lwm2m_os_sleep(int ms);

void lwm2m_os_sys_reset(void);
uint32_t lwm2m_os_rand_get(void);

int lwm2m_os_storage_delete(uint16_t id);
int lwm2m_os_storage_read(uint16_t id, void *data, size_t len);
int lwm2m_os_storage_write(uint16_t id, const void *data, size_t len);

typedef void (*lwm2m_os_timer_handler_t)(void *timer);

/* TODO Get some reasonable source for this config. */
#define LWM2M_OS_MAX_TIMER_COUNT 5

void *lwm2m_os_timer_get(lwm2m_os_timer_handler_t handler);
void lwm2m_os_timer_release(void *timer);
int lwm2m_os_timer_start(void *timer, int32_t timeout);
void lwm2m_os_timer_cancel(void *timer);
int32_t lwm2m_os_timer_remaining(void *timer);

#define LWM2M_LOG_LEVEL_NONE 0
#define LWM2M_LOG_LEVEL_ERR  1
#define LWM2M_LOG_LEVEL_WRN  2
#define LWM2M_LOG_LEVEL_INF  3
#define LWM2M_LOG_LEVEL_TRC  4

const char *lwm2m_os_log_strdup(const char *str);
void lwm2m_os_log(int level, const char *fmt, ...);

#endif // LWM2M_OS_H__
