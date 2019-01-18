#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_library(nrf_mdm_interface_at
    src/lwm2m_at_interface.c
)

# Zephyr Kernel
add_dependencies(nrf_at_cmd_parser offsets_h)

# Add nrf_at_cmd_parser as a dependency.
target_link_libraries(nrf_mdm_interface_at PUBLIC nrf_at_cmd_parser)

target_include_directories(nrf_mdm_interface_at PUBLIC include)
target_link_libraries(nrf_mdm_interface_at PRIVATE zephyr_interface)
