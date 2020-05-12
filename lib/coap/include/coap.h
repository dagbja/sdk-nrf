/*
 * Copyright (c) 2014 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file coap.h
 *
 * @defgroup iot_sdk_coap_api CoAP interface
 * @ingroup iot_sdk_coap
 * @{
 * @brief Interface for the CoAP protocol.
 */

#ifndef COAP_H__
#define COAP_H__

#include <coap_api.h>

/* Use a common header for now. */
#include <lwm2m_os.h>

/**
 * @defgroup iot_coap_log Module's Log Macros
 * @details Macros used for creating module logs which can be useful in
 *          understanding handling of events or actions on API requests.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_NRF_COAP_ENABLE_LOGS)
/** Used for getting trace of execution in the module. */
#define COAP_TRC(...) lwm2m_os_log(LWM2M_LOG_LEVEL_TRC, "coap: " __VA_ARGS__)
/** Used for logging errors in the module. */
#define COAP_ERR(...) lwm2m_os_log(LWM2M_LOG_LEVEL_ERR, "coap: " __VA_ARGS__)
#else /* NRF_COAP_ENABLE_LOGS */
#define COAP_TRC(...)
#define COAP_ERR(...)
#endif /* NRF_COAP_ENABLE_LOGS */


#define COAP_ENTRY() COAP_TRC(">> %s", __func__)
#define COAP_EXIT() COAP_TRC("<< %s", __func__)
#define COAP_EXIT_WITH_RESULT(result) COAP_TRC("<< %s, result: %d", \
					       __func__, (int)result)

#if (COAP_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM) do { \
		if ((PARAM) == NULL) { \
			return EINVAL; \
		} \
	} while (false)

/**@brief Verify that parameter members has been set by the application. */
#define NULL_PARAM_MEMBER_CHECK(PARAM) do { \
		if ((PARAM) == NULL) { \
			return EINVAL; \
		} \
	} while (false)

#else

#define NULL_PARAM_CHECK(PARAM)
#define NULL_PARAM_MEMBER_CHECK(PARAM)

#endif /* COAP_DISABLE_API_PARAM_CHECK */

/**
 * @defgroup iot_coap_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules.
 * @{
 */
/** Lock module using mutex */
#define COAP_MUTEX_LOCK()   /* No mutex for now */

/** Unlock module using mutex */
#define COAP_MUTEX_UNLOCK() /* No mutex for now */

/** @} */

/**@brief Sends a CoAP message.
 *
 * @details Sends out a request using the underlying transport layer. Before
 *          sending, the \ref coap_message_t structure is serialized and added
 *          to an internal message queue in the library. The handle returned
 *          can be used to abort the message from being retransmitted at any
 *          time.
 *
 * @param[out] handle  Handle to the message if CoAP CON/ACK messages has been
 *                     used. Returned by reference.
 * @param[in]  message Message to be sent.
 *
 * @retval 0 If the message was successfully encoded and scheduled for
 *           transmission.
 */
uint32_t internal_coap_message_send(uint32_t *handle, coap_message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* COAP_H__ */

/** @} */