/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_FACTORY_BOOTSTRAP_H__
#define LWM2M_FACTORY_BOOTSTRAP_H__

#define LWM2M_BOOTSTRAP_INSTANCE_ID  0

/**@brief Initialize factory bootstrapped objects. */
void lwm2m_factory_bootstrap_init(void);

/**@brief Update factory bootstrapped objects. */
bool lwm2m_factory_bootstrap_update(lwm2m_carrier_config_t * p_carrier_config);

#endif // LWM2M_FACTORY_BOOTSTRAP_H__