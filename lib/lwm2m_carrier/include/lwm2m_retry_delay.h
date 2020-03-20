/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_RETRY_DELAY_H__
#define LWM2M_RETRY_DELAY_H__

/**@brief Get PDN retry delay.
 *
 * @return Retry delay in milliseconds.
 */
int32_t lwm2m_retry_delay_pdn_get(uint16_t apn_instance, bool * p_is_last);

/**@brief Get PDN retry counter.
 *
 * @return Retry count.
 */
int32_t lwm2m_retry_count_pdn_get(void);

/**@brief Reset PDN retry delay counter.
 */
void lwm2m_retry_delay_pdn_reset(void);

/**@brief Get current retry delay.
 *
 * @param[in]  instance_id  LwM2M security instance ID
 * @param[out] p_is_last    Set to true if this is the last retry delay
 *
 * @return Retry delay in milliseconds. -1 if no more retries.
 */
int32_t lwm2m_retry_delay_connect_get(uint16_t security_instance, bool * p_is_last);

/**@brief Get next retry delay. Increase retry counter.
 *
 * @param[in]  instance_id  LwM2M security instance ID
 * @param[out] p_is_last    Set to true if this is the last retry delay
 *
 * @return Retry delay in milliseconds. -1 if no more retries.
 */
int32_t lwm2m_retry_delay_connect_next(uint16_t security_instance, bool * p_is_last);

/**@brief Reset retry delay counter.
 *
 * @param[in]  instance_id  LwM2M security instance ID
 */
void lwm2m_retry_delay_connect_reset(uint16_t security_instance);

#endif // LWM2M_RETRY_DELAY_H__
