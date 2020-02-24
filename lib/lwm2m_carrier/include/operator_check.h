/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef OPERATOR_CHECK_H__
#define OPERATOR_CHECK_H__

#include <stdint.h>

#define OPERATOR_ID_UNSET   UINT32_MAX
#define OPERATOR_ID_CURRENT UINT32_MAX

/**@brief Read operator id.
 *
 * Read operator id from modem using AT%XOPERID.
 * Store the value for later checks for operator.
 */
void operator_id_read(void);

/**@brief Check if operator is supported.
 *
 * @param[in]  allow_debug  Set if allow debug operator.
 *
 * @return true if operator is supported.
 */
bool operator_is_supported(bool allow_debug);

/**@brief Check if operator is Verizon.
 *
 * @param[in]  allow_debug  Set if allow debug operator.
 *
 * @return true if operator is Verizon.
 */
bool operator_is_vzw(bool allow_debug);

/**@brief Check if operator is AT&T.
 *
 * @param[in]  allow_debug  Set if allow debug operator.
 *
 * @return true if operator is AT&T.
 */
bool operator_is_att(bool allow_debug);

/**@brief Get operator id.
 *
 * @param[in]  allow_debug  Set if allow debug operator.
 *
 * @return Operator id.
 */
uint32_t operator_id(bool allow_debug);

/**@brief Get a string representation of the operator id.
 *
 * @param[in]  operator_id  Operator id, or OPERATOR_ID_CURRENT for current
 *                          operator id loaded in operator_has_changed().
 *
 * @return String with operator id.
 */
const char * operator_id_string(uint32_t operator_id);

/**@brief Get the maximum operator id supported.
 *
 * @return Maximum operator id.
 */
uint32_t operator_id_max(void);

#endif // OPERATOR_CHECK_H__
