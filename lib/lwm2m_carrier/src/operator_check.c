/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <at_interface.h>
#include <operator_check.h>
#include <app_debug.h>

#define OPERATOR_ID_NOT_IDENTIFIED     0        /**< Operator id is not identified. */
#define OPERATOR_ID_VZW                1        /**< Operator id for Verizon SIM. */
#define OPERATOR_ID_ATT                2        /**< Operator id for AT&T SIM. */
#define OPERATOR_ID_ATT_FIRSTNET       3        /**< Operator id for AT&T Firstnet SIM. */
#define OPERATOR_ID_ATT_CRICKET        4        /**< Operator id for AT&T Cricket SIM. */
#define OPERATOR_ID_ATT_JASPER         5        /**< Operator id for AT&T Jasper SIM. */
#define OPERATOR_ID_CHINA_TELECOM      6        /**< Operator id for China Telecom SIM. */
#define OPERATOR_ID_SOFTBANK           7        /**< Operator id for Softbank SIM. */
#define OPERATOR_ID_TELSTRA            8        /**< Operator id for Telstra SIM. */
#define OPERATOR_ID_BELL               9        /**< Operator id for Bell CA SIM. */
#define OPERATOR_ID_LGU               10        /**< Operator id for LGU+ SIM. */
#define OPERATOR_ID_MAX               10        /**< The highest operator id supported. */
/* Note: When adding operators also update operator_id_string() */

/** @brief Operator id from last read. */
static uint32_t m_operator_id;

static bool operator_is_custom(bool allow_debug);

static bool is_not_identified(uint32_t operator_id)
{
    return (operator_id == OPERATOR_ID_NOT_IDENTIFIED);
}

static bool is_vzw(uint32_t operator_id)
{
    return (operator_id == OPERATOR_ID_VZW);
}

static bool is_att(uint32_t operator_id)
{
    return ((operator_id == OPERATOR_ID_ATT)          ||
            (operator_id == OPERATOR_ID_ATT_FIRSTNET) ||
            (operator_id == OPERATOR_ID_ATT_CRICKET)  ||
            (operator_id == OPERATOR_ID_ATT_JASPER));
}

static bool is_lgu(uint32_t operator_id)
{
    return (operator_id == OPERATOR_ID_LGU);
}

void operator_id_read(void)
{
    at_read_operator_id(&m_operator_id);
}

bool operator_is_supported(bool allow_debug)
{
    return (operator_is_vzw(allow_debug) ||
            operator_is_att(allow_debug) ||
            operator_is_lgu(allow_debug) ||
            operator_is_custom(allow_debug));
}

static bool operator_is_custom(bool allow_debug)
{
    // Custom is only supported when carrier check is disabled

    if (allow_debug && lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        return (lwm2m_debug_operator_id_get() == OPERATOR_ID_NOT_IDENTIFIED);
    }

    return false;
}

bool operator_is_vzw(bool allow_debug)
{
    if (is_vzw(m_operator_id)) {
        return true;
    }

    if (allow_debug && lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        return is_not_identified(m_operator_id) && is_vzw(lwm2m_debug_operator_id_get());
    }

    return false;
}

bool operator_is_att(bool allow_debug)
{
    if (is_att(m_operator_id)) {
        return true;
    }

    if (allow_debug && lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        return is_not_identified(m_operator_id) && is_att(lwm2m_debug_operator_id_get());
    }

    return false;
}

bool operator_is_lgu(bool allow_debug)
{
    if (is_lgu(m_operator_id)) {
        return true;
    }

    if (allow_debug && lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        return is_not_identified(m_operator_id) && is_lgu(lwm2m_debug_operator_id_get());
    }

    return false;
}

uint32_t operator_id(bool allow_debug)
{
    if (operator_is_supported(false)) {
        return m_operator_id;
    }

    if (allow_debug && lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        return lwm2m_debug_operator_id_get();
    }

    return m_operator_id;
}

const char * operator_id_string(uint32_t operator_id)
{
    static char unknown[20];

    if (operator_id == UINT32_MAX) {
        operator_id = m_operator_id;
    }

    switch (operator_id) {
        case OPERATOR_ID_NOT_IDENTIFIED:
            return "Not identified";
        case OPERATOR_ID_VZW:
            return "Verizon";
        case OPERATOR_ID_ATT:
            return "AT&T";
        case OPERATOR_ID_ATT_FIRSTNET:
            return "AT&T Firstnet";
        case OPERATOR_ID_ATT_CRICKET:
            return "AT&T Cricket";
        case OPERATOR_ID_ATT_JASPER:
            return "AT&T Jasper";
        case OPERATOR_ID_CHINA_TELECOM:
            return "China Telecom";
        case OPERATOR_ID_SOFTBANK:
            return "Softbank";
        case OPERATOR_ID_TELSTRA:
            return "Telstra";
        case OPERATOR_ID_BELL:
            return "Bell CA";
        case OPERATOR_ID_LGU:
            return "LG U+";
        default:
            break;
    }

    snprintf(unknown, sizeof(unknown), "Unknown: %u", operator_id);

    return unknown;
}

uint32_t operator_id_max(void)
{
    return OPERATOR_ID_MAX;
}