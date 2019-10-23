/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <lwm2m.h>
#include <lwm2m_os.h>
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

	uint32_t tag = CONFIG_NRF_LWM2M_VZW_SEC_TAG;

	if (CONFIG_NRF_LWM2M_VZW_SEC_TAG == -1) {
		LWM2M_WRN("No certificates to be provisioned.");
		return 0;
	}

	err = lwm2m_os_sec_ca_chain_exists(tag, &provisioned, &dummy);
	if (!err && provisioned) {
		LWM2M_INF("Certificates found, tag %lu", tag);
		return 0;
	}

	err = lwm2m_os_sec_ca_chain_write(tag, (char *)ca_chain, sizeof(ca_chain) - 1);

	if (err) {
		LWM2M_ERR("Unable to provision certificate, err: %d", err);
		return err;
	}

	LWM2M_INF("Provisioned certificate, tag %lu", tag);

	return 0;
}