#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_sources_ifdef(CONFIG_AT_CMD_PARSER
    src/at_cmd_parser.c
    src/at_utils.c
)

zephyr_include_directories(include)

