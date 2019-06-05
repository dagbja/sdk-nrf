#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_subdirectory_ifdef(CONFIG_NRF_COAP      coap)
add_subdirectory_ifdef(CONFIG_NRF_LWM2M     lwm2m)
add_subdirectory_ifdef(CONFIG_NRF_LWM2M_VZW lwm2m_vzw)
