# Copyright (c) 2019 - 2021 Nordic Semiconductor ASA. All Rights Reserved.
#
# The information contained herein is confidential property of Nordic Semiconductor ASA.
# The use, copying, transfer or disclosure of such information is prohibited except by
# express written agreement with Nordic Semiconductor ASA.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(example)

# generate runner for the test
test_runner_generate(src/unity_test.c)

# create mock for dependencies
##cmock_handle(../../../../nrfxlib/nrf_modem/include/nrf_socket.h)

# add necessary includes
target_include_directories(app PRIVATE ../../..)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/lwm2m/include)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/lwm2m/src)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/lwm2m_carrier/src)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/lwm2m_carrier/include)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/coap/include)
target_include_directories(app PRIVATE /home/dabj/ncs/lwm2m/lib/coap/src)

# add test file
target_sources(app PRIVATE src/unity_test.c)