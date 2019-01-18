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

# Zephyr Kernel
add_dependencies(nrf_at_params offsets_h)
