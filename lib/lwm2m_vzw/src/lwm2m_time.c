/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <at_interface.h>
#include <lwm2m.h>

#define MAX_TIMEZONE_LEN 64

static int64_t m_current_time_msecs;
static int64_t m_time_base_msecs;

static int m_utc_offset;
static char m_timezone[MAX_TIMEZONE_LEN];

static bool m_time_set = false;
static bool m_utc_offset_set = false;
static bool m_timezone_set = false;

struct tz
{
    int tz_offset;
    const char *tz_string;
};

static const struct tz tz_conv[] =
{
    { 660, "Pacific/Niue" },
    { 600, "Pacific/Honolulu" },
    { 570, "Pacific/Marquesas" },
    { 540, "America/Anchorage" },
    { 480, "America/Los_Angeles" },
    { 420, "America/Phoenix" },
    { 360, "America/Chicago" },
    { 300, "America/New_York" },
    { 240, "America/Santiago" },
    { 210, "America/St_Johns" },
    { 180, "America/Buenos_Aires" },
    { 120, "America/Noronha" },
    { 60, "Atlantic/Azores" },
    { 0, "Europe/Lisbon" },
    { -60, "Europe/Paris" },
    { -120, "Europe/Helsinki" },
    { -180, "Europe/Moscow" },
    { -210, "Asia/Tehran" },
    { -240, "Asia/Dubai" },
    { -270, "Asia/Kabul" },
    { -300, "Asia/Karachi" },
    { -330, "Asia/Kolkata" },
    { -345, "Asia/Kathmandu" },
    { -360, "Asia/Almaty" },
    { -390, "Asia/Yangon" },
    { -420, "Asia/Bangkok" },
    { -480, "Asia/Shanghai" },
    { -525, "Australia/Eucla" },
    { -540, "Asia/Tokyo" },
    { -570, "Australia/Darwin" },
    { -600, "Australia/Sydney" },
    { -630, "Australia/Lord_Howe" },
    { -660, "Pacific/Norfolk" },
    { -720, "Asia/Kamchatka" },
    { -765, "Pacific/Chatham" },
    { -780, "Pacific/Enderbury" },
    { -1, NULL }
};

/**@brief Lookup time zone based on tz offset.
 *
 * @param[in] tz_offset UTC offset in minutes, west of GMT
 * @param[in] dst       Daylight saving adjustment in minutes
 *
 * @return Pointer to timezone string or NULL in case lookup failed
 *
 * */
static const char *lwm2m_time_timezone(int32_t tz_offset, int32_t dst)
{
    int i = 0;
    int offset = tz_offset;

    if (tz_offset <= 0)
    {
        offset += dst;
    }
    else
    {
        offset -= dst;
    }

    while (tz_conv[i].tz_string != NULL)
    {
        if ((offset) == tz_conv[i].tz_offset)
        {
            return tz_conv[i].tz_string;
        }
        i++;
    }

    return NULL;
}

/**@brief Update time, UTC offset and timezone based on modem time. */
static int lwm2m_time_modem_time_get(void)
{
    int32_t utc_offset_15min;
    int32_t time;
    int32_t dst_adjustment;

    int err = at_read_time(&time, &utc_offset_15min, &dst_adjustment);

    /* Check that no err and time makes sense - i.e. time after 2019-01-01T00:00:00 */
    if (err == 0 && time > 1546300800)
    {
        if (m_time_set == false)
        {
            m_current_time_msecs = (int64_t) time * 1000;
            m_time_set = true;
        }

        if (m_utc_offset_set == false)
        {
            m_utc_offset = utc_offset_15min * 15;
        }

        if (m_timezone_set == false)
        {
            /* Give offset in minutes WEST OF GMT */
            const char *p_tz_string = lwm2m_time_timezone(-m_utc_offset, dst_adjustment * 60);

            if (p_tz_string == NULL)
            {
                if (m_utc_offset != 0)
                {
                    /* This simple conversion will loose the timezones containing +-15 +-30min offsets */
                    snprintf(m_timezone, sizeof(m_timezone), "Etc/GMT%+d",
                            (int) -utc_offset_15min / 4);
                }
                else
                {
                    snprintf(m_timezone, sizeof(m_timezone), "Etc/GMT");
                }
            }
            else
            {
                strncpy(m_timezone, p_tz_string, sizeof(m_timezone));
            }
        }
    }

    return err;
}

/**@brief Update current time. */
static void lwm2m_time_current_time_update(void)
{
    if (m_time_set == false)
    {
        int err = lwm2m_time_modem_time_get();

        if (err == 0)
        {
            m_time_base_msecs = lwm2m_os_uptime_get();
        }
        else
        {
            int64_t delta_time = lwm2m_os_uptime_delta(&m_time_base_msecs);
            m_current_time_msecs += delta_time;
        }
    }
    else
    {
        int64_t delta_time = lwm2m_os_uptime_delta(&m_time_base_msecs);
        m_current_time_msecs += delta_time;
    }
}

int32_t __WEAK lwm2m_carrier_utc_time_read(void)
{
    lwm2m_time_current_time_update();

    return (int32_t) (m_current_time_msecs / 1000);
}

int __WEAK lwm2m_carrier_utc_time_write(int32_t time)
{
    if (time >= 0)
    {
        m_current_time_msecs = (int64_t)time * 1000;
        m_time_base_msecs = lwm2m_os_uptime_get();
        m_time_set = true;
    }
    else
    {
        return -EINVAL;
    }

    return 0;
}

int __WEAK lwm2m_carrier_utc_offset_read(void)
{
    if (m_utc_offset_set == false)
    {
        lwm2m_time_modem_time_get();
    }

    return m_utc_offset;
}

int __WEAK lwm2m_carrier_utc_offset_write(int offset)
{
    m_utc_offset = offset;
    m_utc_offset_set = true;

    return 0;
}

const char * __WEAK lwm2m_carrier_timezone_read(void)
{
    if (m_timezone_set == false)
    {
        lwm2m_time_modem_time_get();
    }

    return m_timezone;
}

int __WEAK lwm2m_carrier_timezone_write(const char *p_tz)
{
    strncpy(m_timezone, p_tz, sizeof(m_timezone));
    m_timezone_set = true;

    return 0;
}
