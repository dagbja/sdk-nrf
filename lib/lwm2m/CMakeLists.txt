#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library_named(nrf_lwm2m)

if (CONFIG_NRF_LWM2M_STANDALONE_LIBRARY)
zephyr_library_cc_option(
    -nostdlib
    -nodefaultlibs
    -nostartfiles
    -lnosys
)
endif()

if (NOT CONFIG_NRF_LWM2M_INCLUDE_DEBUG_INFORMATION)
zephyr_library_cc_option(
    -g0
)
endif()

zephyr_include_directories(include)

zephyr_library_sources(
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
