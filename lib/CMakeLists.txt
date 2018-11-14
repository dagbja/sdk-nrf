#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_library(nrf_lwm2m
    src/lwm2m.c
    src/lwm2m_acl.c
    src/lwm2m_bootstrap.c
    src/lwm2m_coap_util.c
    src/lwm2m_objects.c
    src/lwm2m_objects_plain_text.c
    src/lwm2m_objects_tlv.c
    src/lwm2m_register.c
    src/lwm2m_remote.c
    src/lwm2m_tlv.c
)
target_include_directories(nrf_lwm2m PUBLIC include)
add_dependencies(nrf_lwm2m offsets_h)
target_link_libraries(nrf_lwm2m PRIVATE zephyr_interface)
