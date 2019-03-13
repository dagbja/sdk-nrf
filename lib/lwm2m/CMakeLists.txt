#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_sources(
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

zephyr_include_directories(include)

