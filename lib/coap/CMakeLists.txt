#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library_named(nrf_coap)

if (CONFIG_NRF_COAP_STANDALONE_LIBRARY)
zephyr_library_cc_option(
    -nostdlib
    -nodefaultlibs
    -nostartfiles
    -lnosys
)
endif()

if (NOT CONFIG_NRF_COAP_INCLUDE_DEBUG_INFORMATION)
zephyr_library_cc_option(
    -g0
)
endif()

zephyr_include_directories(include)

zephyr_library_sources(
    src/coap_block.c
    src/coap_message.c
    src/coap_observe.c
    src/coap_option.c
    src/coap_queue.c
    src/coap_resource.c
    src/coap_transport_socket.c
    src/coap.c
)
