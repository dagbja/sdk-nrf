/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_VERSION_H__
#define LWM2M_VERSION_H__

#include <toolchain/common.h> // STRINGIFY

#define LWM2M_VERSION_PREFIX "LwM2M "

#define LWM2M_VERSION_MAJOR 0
#define LWM2M_VERSION_MINOR 9
//#define LWM2M_VERSION_PATCH

#if defined(LWM2M_VERSION_PATCH)
#define LWM2M_VERSION_STR \
    (LWM2M_VERSION_PREFIX STRINGIFY(LWM2M_VERSION_MAJOR) \
     "." STRINGIFY(LWM2M_VERSION_MINOR) \
     "-" STRINGIFY(LWM2M_VERSION_PATCH))
#else
#define LWM2M_VERSION_STR \
    (LWM2M_VERSION_PREFIX STRINGIFY(LWM2M_VERSION_MAJOR) \
     "." STRINGIFY(LWM2M_VERSION_MINOR))
#endif

#endif // LWM2M_VERSION_H__
