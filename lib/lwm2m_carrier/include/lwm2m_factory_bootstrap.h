/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_FACTORY_BOOTSTRAP_H__
#define LWM2M_FACTORY_BOOTSTRAP_H__

#include <lwm2m_carrier.h>

#define LWM2M_BOOTSTRAP_INSTANCE_ID  0

#define LWM2M_VZW_BOOTSTRAP_SSID    100   // Verizon Bootstrap server SSID
#define LWM2M_VZW_MANAGEMENT_SSID   102   // Verizon Management server SSID
#define LWM2M_VZW_DIAGNOSTICS_SSID  101   // Verizon Diagnostics server SSID
#define LWM2M_VZW_REPOSITORY_SSID  1000   // Verizon Repository server SSID

/**@brief Initialize factory bootstrapped objects. */
void lwm2m_factory_bootstrap_init(lwm2m_carrier_config_t * p_carrier_config);

/**@brief Update factory bootstrapped objects. */
bool lwm2m_factory_bootstrap_update(lwm2m_carrier_config_t * p_carrier_config,
                                    bool application_psk_set);

#endif // LWM2M_FACTORY_BOOTSTRAP_H__