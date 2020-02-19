#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

# This sample runs as a non-secure application on nRF91. Therefore, it
# requires the secure_partition_manager that prepares the required
# peripherals to be available for the application.
#
# Configure the SPM image to enable the peripherals that this sample
# needs.
set(spm_CONF_FILE
  prj.conf
  ${CMAKE_CURRENT_LIST_DIR}/../client/child_secure_partition_manager.conf
  )

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_sources(app PRIVATE ../client/src/buttons_and_leds.c)
target_sources(app PRIVATE ../client/src/lwm2m_shell.c)
target_sources(app PRIVATE ../client/src/main.c)
target_sources(app PRIVATE ../client/src/modem_logging.c)

zephyr_include_directories(../client/src)

# TODO We keep the lwm2m_carrier headers internal for now
zephyr_include_directories(../lib/lwm2m_carrier/include)
zephyr_include_directories(../lib/lwm2m/include)
zephyr_include_directories(../lib/coap/include)
