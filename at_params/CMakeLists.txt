#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_library(nrf_at_params
    src/at_params.c
)
target_include_directories(nrf_at_params PUBLIC include)
target_link_libraries(nrf_at_params PRIVATE zephyr_interface)

add_dependencies(nrf_at_params offsets_h)

# Zephyr style, does not work, syscall_list.h not found
# cmake_minimum_required(VERSION 3.10)
# zephyr_library()
# zephyr_include_directories(include)
# zephyr_library_sources(src/at_params.c)