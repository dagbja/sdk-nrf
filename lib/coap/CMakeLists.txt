#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library_named(nrf_coap)

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
