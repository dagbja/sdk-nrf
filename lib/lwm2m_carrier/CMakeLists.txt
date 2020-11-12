#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library_named(nrf_lwm2m_carrier)

if (CONFIG_NRF_LWM2M_CARRIER_STANDALONE_LIBRARY)
zephyr_library_cc_option(
    -nostdlib
    -nodefaultlibs
    -nostartfiles
    -lnosys
)
endif()

if (NOT CONFIG_NRF_LWM2M_CARRIER_INCLUDE_DEBUG_INFORMATION)
zephyr_library_cc_option(
    -g0
    -fno-function-sections
    -fno-data-sections
)
endif()

zephyr_include_directories(include)

zephyr_library_sources(
    src/cert.c
    src/app_debug.c
    src/at_interface.c
    src/dfusock.c
    src/lwm2m_access_control.c
    src/lwm2m_apn_conn_prof.c
    src/lwm2m_carrier_main.c
    src/lwm2m_carrier_client.c
    src/lwm2m_client_util.c
    src/lwm2m_conn_ext.c
    src/lwm2m_conn_mon.c
    src/lwm2m_conn_stat.c
    src/lwm2m_factory_bootstrap.c
    src/lwm2m_device.c
    #src/lwm2m_gsma_objects.c
    src/lwm2m_firmware.c
    src/lwm2m_firmware_download.c
    src/lwm2m_instance_storage.c
    src/lwm2m_portfolio.c
    src/lwm2m_retry_delay.c
    src/lwm2m_security.c
    src/lwm2m_server.c
    src/lwm2m_time.c
    src/operator_check.c
    src/sha256.c
    src/sms_receive.c
    src/lwm2m_carrier_api.c
    src/lwm2m_pdn.c
)

if (NOT CONFIG_NRF_LWM2M_CARRIER_STANDALONE_LIBRARY)
zephyr_library_sources(
    src/lwm2m_os.c
)
endif()
