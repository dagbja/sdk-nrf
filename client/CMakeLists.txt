#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_debug.c)
target_sources(app PRIVATE src/at_interface.c)
target_sources(app PRIVATE src/buttons_and_leds.c)
target_sources(app PRIVATE src/lwm2m_conn_mon.c)
target_sources(app PRIVATE src/lwm2m_server.c)
target_sources(app PRIVATE src/lwm2m_security.c)
target_sources(app PRIVATE src/lwm2m_device.c)
target_sources(app PRIVATE src/lwm2m_firmware.c)
target_sources(app PRIVATE src/lwm2m_instance_storage.c)
target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/lwm2m_shell.c)
target_sources(app PRIVATE src/lwm2m_retry_delay.c)

zephyr_include_directories(src)

