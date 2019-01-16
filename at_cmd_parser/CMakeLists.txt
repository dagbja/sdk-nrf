#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_library(nrf_at_cmd_parser
    src/at_cmd_parser.c
    src/at_utils.c
)

add_dependencies(nrf_at_cmd_parser offsets_h)

# Add nrf_at_params a dependency.
target_link_libraries(nrf_at_cmd_parser PUBLIC nrf_at_params)

target_include_directories(nrf_at_cmd_parser PUBLIC include)
target_link_libraries(nrf_at_cmd_parser PRIVATE zephyr_interface)