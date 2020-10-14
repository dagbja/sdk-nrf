/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <lwm2m.h>
#include <lwm2m_os.h>
#include <toolchain.h>

#define ORANGE "\e[0;33m"
#define GREEN "\e[0;32m"

static const char ca_chain[] = {
	#include "../certs/DigiCertGlobalRootG2.pem"	/* VzW and Motive */
	#include "../certs/DSTRootCA-X3.pem"		/* AT&T interop */
};

BUILD_ASSERT(sizeof(ca_chain) < 4096, "CA chain is too large");

int cert_provision(void)
{
	int err;
	bool provisioned;

	uint8_t dummy;
	ARG_UNUSED(dummy);

	uint32_t tag = CONFIG_NRF_LWM2M_CARRIER_SEC_TAG;

	if (CONFIG_NRF_LWM2M_CARRIER_SEC_TAG == -1) {
		LWM2M_WRN("No certificates to be provisioned.");
		return 0;
	}

	err = lwm2m_os_sec_ca_chain_exists(tag, &provisioned, &dummy);
	if (!err && provisioned) {
		/* 0 on match, 1 otherwise; like memcmp() */
		err = lwm2m_os_sec_ca_chain_cmp(tag, ca_chain,
						sizeof(ca_chain) - 1);

		LWM2M_INF("Certificate found, tag %d: %s", tag,
			  err ? ORANGE "mismatch" : GREEN "match");

		if (!err) {
			return err;
		}

		/* continue to overwrite the certificate */
	}

	err = lwm2m_os_sec_ca_chain_write(tag, ca_chain, sizeof(ca_chain) - 1);
	if (err) {
		LWM2M_ERR("Unable to provision certificate, err: %d", err);
		return err;
	}

	LWM2M_INF("Provisioned certificate, tag %d", tag);

	return 0;
}
