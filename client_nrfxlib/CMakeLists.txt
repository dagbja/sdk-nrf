#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_sources(app PRIVATE ../client/src/buttons_and_leds.c)
target_sources(app PRIVATE ../client/src/lwm2m_shell.c)
target_sources(app PRIVATE ../client/src/main.c)
target_sources(app PRIVATE ../client/src/sms_receive.c)

target_sources(app PRIVATE ../lib/lwm2m_vzw/src/lwm2m_os.c)

zephyr_include_directories(../client/src)

# TODO We keep the lwm2m_vzw headers internal for now
zephyr_include_directories(../lib/lwm2m_vzw/include)
