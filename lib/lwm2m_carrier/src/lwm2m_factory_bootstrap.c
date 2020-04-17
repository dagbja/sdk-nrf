/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_acl.h>
#include <lwm2m_api.h>
#include <lwm2m_carrier.h>
#include <lwm2m_common.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_os.h>
#include <lwm2m_factory_bootstrap.h>

#include <app_debug.h>
#include <operator_check.h>

#define BOOTSTRAP_URI_VZW              "coaps://boot.lwm2m.vzwdm.com:5684"                   /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW            "coaps://diag.lwm2m.vzwdm.com:5684"                   /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_VZW_TEST         "coaps://ddocdpboot.do.motive.com:5684"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW_TEST       ""                                                    /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_ATT              "coaps://bootstrap.dm.iot.att.com:5694"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define BOOTSTRAP_URI_ATT_TEST         "coaps://InteropBootstrap.dm.iot.att.com:5694"        /**< Server URI to the bootstrap server when using security (DTLS). */

// PSK: d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747
#define BOOTSTRAP_SEC_PSK_VZW          { 0xd6, 0x16, 0x0c, 0x2e, 0x7c, 0x90, 0x39, 0x9e, \
                                         0xe7, 0xd2, 0x07, 0xa2, 0x26, 0x11, 0xe3, 0xd3, \
                                         0xa8, 0x72, 0x41, 0xb0, 0x46, 0x29, 0x76, 0xb9, \
                                         0x35, 0x34, 0x1d, 0x00, 0x0a, 0x91, 0xe7, 0x47 }    /**< Pre-shared key used for bootstrap server in hex format. */

// PSK: ccb7e44cb5890c9095157506c650ee05
#define BOOTSTRAP_SEC_PSK_ATT          { 0xcc, 0xb7, 0xe4, 0x4c, 0xb5, 0x89, 0x0c, 0x90, \
                                         0x95, 0x15, 0x75, 0x06, 0xc6, 0x50, 0xee, 0x05 }    /**< Pre-shared key used for bootstrap server in hex format. */

static char m_bootstrap_psk_vzw[] = BOOTSTRAP_SEC_PSK_VZW;
static char m_bootstrap_psk_att[] = BOOTSTRAP_SEC_PSK_ATT;

static const uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

static void factory_security_bootstrap_default(void)
{
    const uint16_t instance_id = LWM2M_BOOTSTRAP_INSTANCE_ID;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
    lwm2m_security_is_bootstrap_server_set(instance_id, true);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_default(void)
{
    const uint16_t instance_id = 0;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID,
        .access = { rwde_access },
        .server = { 123 }
    };

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}

static void factory_security_bootstrap_vzw(void)
{
    const uint16_t instance_id = LWM2M_BOOTSTRAP_INSTANCE_ID;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, 100);
    lwm2m_security_is_bootstrap_server_set(instance_id, true);
    lwm2m_security_bootstrapped_set(instance_id, false);
    lwm2m_security_hold_off_timer_set(instance_id, 10);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_security_diagnostics_vzw(void)
{
    const uint16_t instance_id = 2;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, 101);
    if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        lwm2m_security_server_uri_set(instance_id, DIAGNOSTICS_URI_VZW_TEST, strlen(DIAGNOSTICS_URI_VZW_TEST));
    } else {
        lwm2m_security_server_uri_set(instance_id, DIAGNOSTICS_URI_VZW, strlen(DIAGNOSTICS_URI_VZW));
    }

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_bootstrap_vzw(void)
{
    const uint16_t instance_id = LWM2M_BOOTSTRAP_INSTANCE_ID;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 100,
        .access = { rwde_access },
        .server = { 102 }
    };

    lwm2m_server_short_server_id_set(instance_id, 100);
    lwm2m_server_client_hold_off_timer_set(instance_id, 0);

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_management_vzw(void)
{
    const uint16_t instance_id = 1;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 102,
        .access = { rwde_access, rwde_access, rwde_access },
        .server = { 101, 102, 1000 }
    };

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}

static void factory_server_diagnostics_vzw(void)
{
    const uint16_t instance_id = 2;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 101,
        .access = { rwde_access },
        .server = { 102 }
    };

    lwm2m_server_short_server_id_set(instance_id, 101);
    lwm2m_server_client_hold_off_timer_set(instance_id, 30);
    lwm2m_server_lifetime_set(instance_id, 86400);
    lwm2m_server_min_period_set(instance_id, 300);
    lwm2m_server_max_period_set(instance_id, 6000);
    lwm2m_server_notif_storing_set(instance_id, 1);
    lwm2m_server_binding_set(instance_id, "UQS", 3);

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_repository_vzw(void)
{
    const uint16_t instance_id = 3;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 1000,
        .access = { rwde_access, rwde_access, rwde_access },
        .server = { 101, 102, 1000 }
    };

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}

static void factory_server_att(void)
{
    const uint16_t instance_id = 0;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 1,
        .access = { rwde_access },
        .server = { 1 }
    };

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}

static void factory_server_test_att(void)
{
    const uint16_t instance_id = 2;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_instance_acl_t acl = {
        .owner = 1,
        .access = { rwde_access },
        .server = { 1 }
    };

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}

/**@brief Reset factory bootstrapped objects. */
static void factory_bootstrap_reset(uint16_t instance_id)
{
    lwm2m_security_reset(instance_id);
    lwm2m_server_reset(instance_id);

    // Reset VzW specific values
    lwm2m_security_hold_off_timer_set(instance_id, false);
    lwm2m_security_is_bootstrap_server_set(instance_id, false);
    lwm2m_server_registered_set(instance_id, false);
    lwm2m_server_client_hold_off_timer_set(instance_id, 0);
}

static void factory_bootstrap_bootstrap(void)
{
    factory_bootstrap_reset(LWM2M_BOOTSTRAP_INSTANCE_ID);

    if (operator_is_vzw(true))
    {
        factory_security_bootstrap_vzw();
        factory_server_bootstrap_vzw();
    }
    else
    {
        factory_security_bootstrap_default();
    }
}

void lwm2m_factory_bootstrap_init(void)
{
    // Initialize all instances except Bootstrap server
    for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        factory_bootstrap_reset(i);
    }

    if (operator_is_vzw(true))
    {
        factory_security_diagnostics_vzw();
        factory_server_management_vzw();
        factory_server_diagnostics_vzw();
        factory_server_repository_vzw();
    }
    else if (operator_is_att(true))
    {
        factory_server_att();
        factory_server_test_att();
    }
    else
    {
        factory_server_default();
    }

    lwm2m_storage_security_store();
    lwm2m_storage_server_store();
    lwm2m_storage_acl_store();
}

bool lwm2m_factory_bootstrap_update(lwm2m_carrier_config_t * p_carrier_config)
{
    char * bootstrap_uri = NULL;
    bool   settings_changed = false;

    if (p_carrier_config->bootstrap_uri)
    {
        LWM2M_INF("Setting custom bootstrap: %s", lwm2m_os_log_strdup(p_carrier_config->bootstrap_uri));
        bootstrap_uri = p_carrier_config->bootstrap_uri;
    }
    else if (operator_is_vzw(true))
    {
        LWM2M_INF("Setting VzW bootstrap");
        if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
            // Carrier check is disabled, connect to test servers
            bootstrap_uri = BOOTSTRAP_URI_VZW_TEST;
        } else {
            // Carrier check is enabled, connect to live servers
            bootstrap_uri = BOOTSTRAP_URI_VZW;
        }
        p_carrier_config->psk = m_bootstrap_psk_vzw;
        p_carrier_config->psk_length = sizeof(m_bootstrap_psk_vzw);
    }
    else if (operator_is_att(true))
    {
        LWM2M_INF("Setting AT&T bootstrap");
        if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
            // Carrier check is disabled, connect to test servers
            bootstrap_uri = BOOTSTRAP_URI_ATT_TEST;
        } else {
            // Carrier check is enabled, connect to live servers
            bootstrap_uri = BOOTSTRAP_URI_ATT;
        }
        p_carrier_config->psk = m_bootstrap_psk_att;
        p_carrier_config->psk_length = sizeof(m_bootstrap_psk_att);
    }
    else
    {
        bootstrap_uri = CONFIG_NRF_LWM2M_CARRIER_BOOTSTRAP_URI;
        p_carrier_config->psk = CONFIG_NRF_LWM2M_CARRIER_BOOTSTRAP_PSK;
        p_carrier_config->psk_length = sizeof(CONFIG_NRF_LWM2M_CARRIER_BOOTSTRAP_PSK) - 1;
    }

    // If there is a debug PSK available in flash, apply it instead of the PSK above.
    lwm2m_string_t debug_psk;
    int32_t ret = lwm2m_debug_bootstrap_psk_get(&debug_psk);
    if (ret == 0)
    {
        LWM2M_INF("Using debug bootstrap PSK");
        p_carrier_config->psk = debug_psk.p_val;
        p_carrier_config->psk_length = (size_t)debug_psk.len;
    }

    uint8_t p_len = 0;
    char * p = lwm2m_security_server_uri_get(LWM2M_BOOTSTRAP_INSTANCE_ID, &p_len);
    if ((bootstrap_uri) &&
        (strlen(bootstrap_uri) > 0) &&
        (p_len == 0 || strncmp(p, bootstrap_uri, p_len) != 0))
    {
        // Initial startup (no server URI) or server URI has changed (carrier changed).
        // Clear all bootstrap settings and load factory settings.
        factory_bootstrap_bootstrap();

        lwm2m_security_server_uri_set(LWM2M_BOOTSTRAP_INSTANCE_ID, bootstrap_uri, strlen(bootstrap_uri));

        lwm2m_storage_server_store();
        lwm2m_storage_security_store();

        settings_changed = true;
    }

    return settings_changed;
}