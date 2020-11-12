/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**
 * @file lwm2m_pdn.h
 *
 * @brief API for the PDN management.
 */

#ifndef LWM2M_PDN_H__
#define LWM2M_PDN_H__

/**
 * @brief PDN Context ID max value.
 */
#define MAX_NUM_OF_PDN_CONTEXTS (25)

/**
 * @brief Socket descriptor value used as a default PDN.
 */
#define DEFAULT_PDN_FD (-1)

void lwm2m_pdn_init(void);
bool lwm2m_pdn_activate(bool *p_pdn_activated, nrf_sa_family_t *p_pdn_type_allowed);
void lwm2m_pdn_deactivate(void);
void lwm2m_pdn_check_closed(void);
bool lwm2m_pdn_first_enabled_apn_instance(void);
bool lwm2m_pdn_next_enabled_apn_instance(void);
nrf_sa_family_t lwm2m_pdn_type_allowed(void);
uint16_t lwm2m_apn_instance(void);
char *lwm2m_pdn_current_apn(void);
char *lwm2m_pdn_default_apn(void);

#endif /* LWM2M_PDN_H__ */
