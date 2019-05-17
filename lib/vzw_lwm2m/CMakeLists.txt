#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library_named(nrf_vzw_lwm2m)

if(!CONFIG_NRF_VZW_LWM2M_INCLUDE_DEBUG_INFORMATION)
zephyr_library_cc_option(
    -g0
)
endif()

zephyr_include_directories(include)

# TODO Temporary solution until some app interface is defined
zephyr_library_include_directories(../../client/src)

zephyr_library_sources(
    src/common.c
    src/lwm2m_conn_mon.c
    src/lwm2m_device.c
    src/lwm2m_firmware.c
    src/lwm2m_instance_storage.c
    src/lwm2m_retry_delay.c
    src/lwm2m_security.c
    src/lwm2m_server.c
    )
