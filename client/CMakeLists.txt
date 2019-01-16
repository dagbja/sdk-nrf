#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/lwm2m_conn_mon.c)
target_sources(app PRIVATE src/lwm2m_server.c)
target_sources(app PRIVATE src/lwm2m_security.c)
target_sources(app PRIVATE src/lwm2m_device.c)
target_sources(app PRIVATE src/common.c)
zephyr_include_directories(src)

target_link_libraries(app PRIVATE nrf_at_params)

add_subdirectory(.. ${PROJECT_BINARY_DIR}/zephyr/nrf_lwm2m)
target_link_libraries(app PRIVATE nrf_lwm2m)

