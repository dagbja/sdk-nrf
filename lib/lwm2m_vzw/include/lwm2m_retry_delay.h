/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_RETRY_DELAY_H__
#define LWM2M_RETRY_DELAY_H__

/**@brief Get current retry delay. Increase retry counter if requested.
 *
 * @param[in]  instance_id  LwM2M server instance ID
 * @param[in]  next_delay   Fetch next delay
 *
 * @return Retry delay in seconds. -1 if no more retries.
 */
int32_t lwm2m_retry_delay_get(int instance_id, bool next_delay);

/**@brief Reset retry delay counter.
 *
 * @param[in]  instance_id  LwM2M server instance ID
 */
void lwm2m_retry_delay_reset(int instance_id);

#endif // LWM2M_RETRY_DELAY_H__
