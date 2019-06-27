/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <logging/log.h>
#include <nrf_inbuilt_key.h>
#include <nrf_key_mgmt.h>

#if 0
static const char motive[] = {
"-----BEGIN CERTIFICATE-----\n"
"MIIGszCCBZugAwIBAgIQAwteyFvVCv6KUtPkXpY8ezANBgkqhkiG9w0BAQsFADBEMQswCQYDVQQG\n"
"EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMR4wHAYDVQQDExVEaWdpQ2VydCBHbG9iYWwgQ0Eg\n"
"RzIwHhcNMTgwOTEwMDAwMDAwWhcNMjAwOTEwMTIwMDAwWjCBhDELMAkGA1UEBhMCVVMxCzAJBgNV\n"
"BAgTAkNBMRIwEAYDVQQHEwlDYWxhYmFzYXMxGzAZBgNVBAoTEkFsY2F0ZWwtTHVjZW50IFVTQTEY\n"
"MBYGA1UECxMPSG9zdGVkIFNlcnZpY2VzMR0wGwYDVQQDExRkZG9jZHAuZG8ubW90aXZlLmNvbTCC\n"
"ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANNsV3LxOtNuva4W3LERLH9yCJHnYLjG91Zh\n"
"UPlZGA/lZjW5dZaHxDzGoBm9/IrQDMxQVK+IR+phuKLxJo1rAyAZY2eMy/9qO7q7tkwnQ6Ut1LLn\n"
"kLItE6Z46AOC5rcUX9I3ngfLTe78QYy5MPVsivoeDAHCJH7tAhPGviyZI/LCjlznJ9m04Plr/92c\n"
"BUTMCHE+KWbZOvK5R53DI63jCLOgAUwzxgoWdIxNiya1UOB4yAz/P2wVqXstmAqNhG6A5phIjQbK\n"
"kCMxSv0lkCPrDUyae+dkkk/vS72qBhlaWbxYwd2Bv0s1QeLhx9vo6+fKN2NGf/aCskUHH/igniYr\n"
"TbUCAwEAAaOCA14wggNaMB8GA1UdIwQYMBaAFCRuKy3QapJRUSVpAaqaR6aJ50AgMB0GA1UdDgQW\n"
"BBSWHRdi8ILafmcz5TXNN6fCpkWnqjAfBgNVHREEGDAWghRkZG9jZHAuZG8ubW90aXZlLmNvbTAO\n"
"BgNVHQ8BAf8EBAMCBaAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMHcGA1UdHwRwMG4w\n"
"NaAzoDGGL2h0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbENBRzIuY3JsMDWg\n"
"M6Axhi9odHRwOi8vY3JsNC5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9iYWxDQUcyLmNybDBMBgNV\n"
"HSAERTBDMDcGCWCGSAGG/WwBATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5j\n"
"b20vQ1BTMAgGBmeBDAECAjB0BggrBgEFBQcBAQRoMGYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3Nw\n"
"LmRpZ2ljZXJ0LmNvbTA+BggrBgEFBQcwAoYyaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0Rp\n"
"Z2lDZXJ0R2xvYmFsQ0FHMi5jcnQwCQYDVR0TBAIwADCCAX4GCisGAQQB1nkCBAIEggFuBIIBagFo\n"
"AHUApLkJkLQYWBSHuxOizGdwCjw1mAT5G9+443fNDsgN3BAAAAFlxLe34QAABAMARjBEAiAO5HVc\n"
"3MIMedGxJJHGeUdFTbp6sOkj24LAanH8TtqWmwIgaG93ciA45FgQ4xOa7MLIHVob9mku0spqlhO3\n"
"s7GVgIkAdwCHdb/nWXz4jEOZX73zbv9WjUdWNv9KtWDBtOr/XqCDDwAAAWXEt7ioAAAEAwBIMEYC\n"
"IQCUDN8sOxOUOxbPEt2vWTAljJfss2zCF0yTro9zJ1hjxQIhAKq1P3YkHIneicUPTecifg5EcsLx\n"
"+qSzd0xXaLyKxYjeAHYAu9nfvB+KcbWTlCOXqpJ7RzhXlQqrUugakJZkNo4e0YUAAAFlxLe33AAA\n"
"BAMARzBFAiEAyVdnxGOSCWw6UgxCO0N7JudH8PEH1s8T51bY6eg0C18CIAo/3xnBewONhYmolnSC\n"
"c4t2cmLaV/wUXTcSriYlrr9iMA0GCSqGSIb3DQEBCwUAA4IBAQA8+B1xfqjk3bzY4NbO8Vu+IVQY\n"
"pWCey3t9NDuzO3KnP6XszChqA7awhoxwMGghTjcFqUha0kTHBQ7Im/3TQzwPG1BAGPgCuKdt0j2N\n"
"ohlKnIJ5juQb7Mkmc+2ErEg/iYB/ZlSTDmU4JU4NBRDaun2x3u/mh4EM/X5WxYO0s06m2W6JwD/U\n"
"vX9w2eawqCZvLBlk6Z2VdY9KQKGqGIXCzXx+vKp8nH2nIeXN3h+FFN88sF3uFFirji6MljNaqIgz\n"
"VO+fFXX7ylbedaW+4q3BG8h2p/3VXcWpZWLVnfpOFm20Xoe+tLiEcTVOovkhHPt738VkpOiu6T8e\n"
"0tjLoIoerlzq\n"
"-----END CERTIFICATE-----"
};

/* DigiCert Global CA G2 */
static const char digicert_global_ca_g2[] = {
"-----BEGIN CERTIFICATE-----\n"
"MIIEizCCA3OgAwIBAgIQDI7gyQ1qiRWIBAYe4kH5rzANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQG\n"
"EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAw\n"
"HgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0yODA4MDEx\n"
"MjAwMDBaMEQxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxHjAcBgNVBAMTFURp\n"
"Z2lDZXJ0IEdsb2JhbCBDQSBHMjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANNIfL7z\n"
"BYZdW9UvhU5L4IatFaxhz1uvPmoKR/uadpFgC4przc/cV35gmAvkVNlW7SHMArZagV+Xau4CLyMn\n"
"uG3UsOcGAngLH1ypmTb+u6wbBfpXzYEQQGfWMItYNdSWYb7QjHqXnxr5IuYUL6nG6AEfq/gmD6yO\n"
"TSwyOR2Bm40cZbIc22GoiS9g5+vCShjEbyrpEJIJ7RfRACvmfe8EiRROM6GyD5eHn7OgzS+8LOy4\n"
"g2gxPR/VSpAQGQuBldYpdlH5NnbQtwl6OErXb4y/E3w57bqukPyV93t4CTZedJMeJfD/1K2uaGvG\n"
"/w/VNfFVbkhJ+Pi474j48V4Rd6rfArMCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8CAQAw\n"
"DgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3Au\n"
"ZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9E\n"
"aWdpQ2VydEdsb2JhbFJvb3RHMi5jcmwwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9E\n"
"aWdpQ2VydEdsb2JhbFJvb3RHMi5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEW\n"
"HGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFCRuKy3QapJRUSVpAaqaR6aJ\n"
"50AgMB8GA1UdIwQYMBaAFE4iVCAYlebjbuYP+vq5Eu0GF485MA0GCSqGSIb3DQEBCwUAA4IBAQAL\n"
"OYSR+ZfrqoGvhOlaOJL84mxZvzbIRacxAxHhBsCsMsdaVSnaT0AC9aHesO3ewPj2dZ12uYf+QYB6\n"
"z13jAMZbAuabeGLJ3LhimnftiQjXS8X9Q9ViIyfEBFltcT8jW+rZ8uckJ2/0lYDblizkVIvP6hnZ\n"
"f1WZUXoOLRg9eFhSvGNoVwvdRLNXSmDmyHBwW4coatc7TlJFGa8kBpJIERqLrqwYElesA8u49L3K\n"
"Jg6nwd3jM+/AVTANlVlOnAM2BvjAjxSZnE0qnsHhfTuvcqdFuhOWKU4Z0BqYBvQ3lBetoxi6PrAB\n"
"DJXWKTUgNX31EGDk92hiHuwZ4STyhxGs6QiA\n"
"-----END CERTIFICATE-----"
};
#endif

/* DigiCert Global Root G2 */
static const char digicert[] = {
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBhMQswCQYDVQQG\n"
"EwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAw\n"
"HgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUx\n"
"MjAwMDBaMGExCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3\n"
"dy5kaWdpY2VydC5jb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkq\n"
"hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI2/Ou8jqJ\n"
"kTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx1x7e/dfgy5SDN67sH0NO\n"
"3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQq2EGnI/yuum06ZIya7XzV+hdG82MHauV\n"
"BJVJ8zUtluNJbd134/tJS7SsVQepj5WztCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyM\n"
"UNGPHgm+F6HmIcr9g+UQvIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQAB\n"
"o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV5uNu\n"
"5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY1Yl9PMWLSn/pvtsr\n"
"F9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4NeF22d+mQrvHRAiGfzZ0JFrabA0U\n"
"WTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NGFdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBH\n"
"QRFXGU7Aj64GxJUTFy8bJZ918rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/\n"
"iyK5S9kJRaTepLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
"MrY=\n"
"-----END CERTIFICATE-----"
};

#if 0
/*
 * CN=DigiCert Baltimore CA-2 G2, OU=www.digicert.com, O=DigiCert Inc, C=US
 */
static const char amazon[] = {
"-----BEGIN CERTIFICATE-----\n"
"MIIEYzCCA0ugAwIBAgIQAYL4CY6i5ia5GjsnhB+5rzANBgkqhkiG9w0BAQsFADBaMQswCQYDVQQG\n"
"EwJJRTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYDVQQDExlC\n"
"YWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTE1MTIwODEyMDUwN1oXDTI1MDUxMDEyMDAwMFow\n"
"ZDELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3LmRpZ2lj\n"
"ZXJ0LmNvbTEjMCEGA1UEAxMaRGlnaUNlcnQgQmFsdGltb3JlIENBLTIgRzIwggEiMA0GCSqGSIb3\n"
"DQEBAQUAA4IBDwAwggEKAoIBAQC75wD+AAFz75uI8FwIdfBccHMf/7V6H40II/3HwRM/sSEGvU3M\n"
"2y24hxkx3tprDcFd0lHVsF5y1PBm1ITykRhBtQkmsgOWBGmVU/oHTz6+hjpDK7JZtavRuvRZQHJa\n"
"Z7bN5lX8CSukmLK/zKkf1L+Hj4Il/UWAqeydjPl0kM8c+GVQr834RavIL42ONh3e6onNslLZ5QnN\n"
"NnEr2sbQm8b2pFtbObYfAB8ZpPvTvgzm+4/dDoDmpOdaxMAvcu6R84Nnyc3KzkqwIIH95HKvCRjn\n"
"T0LsTSdCTQeg3dUNdfc2YMwmVJihiDfwg/etKVkgz7sl4dWe5vOuwQHrtQaJ4gqPAgMBAAGjggEZ\n"
"MIIBFTAdBgNVHQ4EFgQUwBKyKHRoRmfpcCV0GgBFWwZ9XEQwHwYDVR0jBBgwFoAU5Z1ZMIJHWMys\n"
"+ghUNoZ7OrUETfAwEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwNAYIKwYBBQUH\n"
"AQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wOgYDVR0fBDMwMTAv\n"
"oC2gK4YpaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL09tbmlyb290MjAyNS5jcmwwPQYDVR0gBDYw\n"
"NDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwDQYJ\n"
"KoZIhvcNAQELBQADggEBAC/iN2bDGs+RVe4pFPpQEL6ZjeIo8XQWB2k7RDA99blJ9Wg2/rcwjang\n"
"B0lCY0ZStWnGm0nyGg9Xxva3vqt1jQ2iqzPkYoVDVKtjlAyjU6DqHeSmpqyVDmV47DOMvpQ+2HCr\n"
"6sfheM4zlbv7LFjgikCmbUHY2Nmz+S8CxRtwa+I6hXsdGLDRS5rBbxcQKegOw+FUllSlkZUIII1p\n"
"LJ4vP1C0LuVXH6+kc9KhJLsNkP5FEx2noSnYZgvD0WyzT7QrhExHkOyL4kGJE7YHRndC/bseF/r/\n"
"JUuOUFfrjsxOFT+xJd1BDKCcYm1vupcHi9nzBhDFKdT3uhaQqNBU4UtJx5g=\n"
"-----END CERTIFICATE-----"
};
#endif

LOG_MODULE_REGISTER(cert);

int cert_provision(void)
{
	int err;

	nrf_sec_tag_t sec_tag = CONFIG_NRF_LWM2M_VZW_SEC_TAG;

	if (CONFIG_NRF_LWM2M_VZW_SEC_TAG == -1) {
		LOG_INF("No certificates to be provisioned.");
		return 0;
	}

	/* Here we simply delete and provision the certificates at each boot.
	 * This clearly has implications on the device lifetime.
	 *
	 * TODO:
	 * - fix nrf_inbuilt_key_exists() implementation
	 *   to not return an error when the key does not exist,
	 *   since it has an output parameter
	 * - update the documentation accordingly
	 * - use it here to avoid deleting and
	 *   re-provisioning certificates at each boot
	 */

	/* Delete certificate */
	err = nrf_inbuilt_key_delete(sec_tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN);
	if (err) {
		/* Ignore errors, which can happen if the key doesn't exist. */
	}

#if 0
	err = nrf_inbuilt_key_write(sec_tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    (char *)motive, sizeof(motive) - 1);

	if (err) {
		LOG_ERR("Unable to provision certicate #1, err: %d", err);
		return err;
	}
#endif

#if 0
	err = nrf_inbuilt_key_write(sec_tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    (char *)digicert_global_ca_g2,
				    sizeof(digicert_global_ca_g2) - 1);

	if (err) {
		LOG_ERR("Unable to provision certificate #2, err: %d", err);
		return err;
	}
#endif

#if 1
	err = nrf_inbuilt_key_write(sec_tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    (char *)digicert, sizeof(digicert) - 1);

	if (err) {
		LOG_ERR("Unable to provision certificate #3, err: %d", err);
		return err;
	}

	LOG_INF("Provisioned certificate, tag %lu", sec_tag);
#endif

#if 0
	err = nrf_inbuilt_key_write(sec_tag, NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    (char *)amazon, sizeof(amazon) - 1);

	if (err) {
		LOG_ERR("Unable to provision certificate #4, err: %d", err);
		return err;
	}
#endif

	LOG_INF("Certificates provisioned");

	return 0;
}