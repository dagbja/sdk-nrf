/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m.h>
#include <nrf_inbuilt_key.h>
#include <nrf_key_mgmt.h>
#include <toolchain.h>

static const char ca_chain[] = {
#if 1
	#include "../certs/DigiCertGlobalRootG2.pem" /* Motive */
#else
	#include "../certs/DigiCertGlobalRootCA.pem" /* VzW    */
#endif
};

BUILD_ASSERT_MSG(sizeof(ca_chain) < 4096, "CA is too large");

int cert_provision(void)
{
	int err;
	bool provisioned;

	uint8_t dummy;
	ARG_UNUSED(dummy);

	nrf_sec_tag_t tag = CONFIG_NRF_LWM2M_VZW_SEC_TAG;

	if (CONFIG_NRF_LWM2M_VZW_SEC_TAG == -1) {
		LWM2M_WRN("No certificates to be provisioned.");
		return 0;
	}

	err = nrf_inbuilt_key_exists(tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				     &provisioned, &dummy);
	if (!err && provisioned) {
		LWM2M_INF("Certificates found, tag %lu", tag);
		return 0;
	}

	err = nrf_inbuilt_key_write(tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    (char *)ca_chain, sizeof(ca_chain) - 1);

	if (err) {
		LWM2M_ERR("Unable to provision certificate, err: %d", err);
		return err;
	}

	LWM2M_INF("Provisioned certificate, tag %lu", tag);

	return 0;
}