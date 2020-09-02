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
  ${CMAKE_CURRENT_LIST_DIR}/child_secure_partition_manager.conf
  )

execute_process(
	COMMAND git describe --always --dirty
	OUTPUT_VARIABLE LWM2M_CLIENT_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND hostname
	OUTPUT_VARIABLE LWM2M_CLIENT_BUILD_HOST
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

zephyr_cc_option(-Werror)

target_sources(app PRIVATE src/client_leds.c)
target_sources(app PRIVATE src/client_shell.c)
target_sources(app PRIVATE src/modem_logging.c)

add_definitions(-DLWM2M_CLIENT_VERSION=${LWM2M_CLIENT_VERSION})
add_definitions(-DLWM2M_CLIENT_BUILD_HOST=${LWM2M_CLIENT_BUILD_HOST})
target_sources(app PRIVATE src/main.c)

zephyr_include_directories(src)
