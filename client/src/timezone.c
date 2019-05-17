/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct tz {
    int tz_offset;
    const char *tz_string;
};

static const struct tz tz_conv[] = {
        {660, "Pacific/Niue"},
        {600, "Pacific/Honolulu"},
        {570, "Pacific/Marquesas"},
        {540, "America/Anchorage"},
        {480, "America/Los_Angeles"},
        {420, "America/Phoenix"},
        {360, "America/Chicago"},
        {300, "America/New_York"},
        {240, "America/Santiago"},
        {210, "America/St_Johns"},
        {180, "America/Buenos_Aires"},
        {120, "America/Noronha"},
        {60, "Atlantic/Azores"},
        {0, "Europe/Lisbon"},
        {-60, "Europe/Paris"},
        {-120, "Europe/Helsinki"},
        {-180, "Europe/Moscow"},
        {-210, "Asia/Tehran"},
        {-240, "Asia/Dubai"},
        {-270, "Asia/Kabul"},
        {-300, "Asia/Karachi"},
        {-330, "Asia/Kolkata"},
        {-345, "Asia/Kathmandu"},
        {-360, "Asia/Almaty"},
        {-390, "Asia/Yangon"},
        {-420, "Asia/Bangkok"},
        {-480, "Asia/Shanghai"},
        {-525, "Australia/Eucla"},
        {-540, "Asia/Tokyo"},
        {-570, "Australia/Darwin"},
        {-600, "Australia/Sydney"},
        {-630, "Australia/Lord_Howe"},
        {-660, "Pacific/Norfolk"},
        {-720, "Asia/Kamchatka"},
        {-765, "Pacific/Chatham"},
        {-780, "Pacific/Enderbury"},
        {-1, NULL}
};

const char * lwm2m_carrier_timezone(int32_t tz_offset, int32_t dst)
{
    int i = 0;
    while(tz_conv[i].tz_string != NULL)
    {
        if ((tz_offset - dst) == tz_conv[i].tz_offset)
        {
            return tz_conv[i].tz_string;
        }
        i++;
    }

    return NULL;
}
