/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_carrier.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_instance_storage.h>
#include <lwm2m_os.h>
#include <lwm2m_factory_bootstrap.h>
#include <lwm2m_access_control.h>

#include <app_debug.h>
#include <operator_check.h>

#define BOOTSTRAP_URI_VZW              "coaps://boot.lwm2m.vzwdm.com:5684"                   /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW            "coaps://diag.lwm2m.vzwdm.com:5684"                   /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_VZW_TEST         "coaps://ddocdpboot.do.motive.com:5684"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW_TEST       ""                                                    /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_ATT              "coaps://bootstrap.dm.iot.att.com:5694"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define BOOTSTRAP_URI_ATT_TEST         "coaps://InteropBootstrap.dm.iot.att.com:5694"        /**< Server URI to the bootstrap server when using security (DTLS). */

/** Pre-shared key used for bootstrap server in hex format. */
#define BOOTSTRAP_SEC_PSK_VZW          "d6160c2e7c90399ee7d207a22611e3d3a87241b0462976b935341d000a91e747"

static const uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                                     LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

static void factory_security_bootstrap_default(void)
{
    const uint16_t instance_id = LWM2M_BOOTSTRAP_INSTANCE_ID;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
    lwm2m_security_is_bootstrap_server_set(instance_id, true);
    lwm2m_security_hold_off_timer_set(0);
    lwm2m_security_bootstrapped_set(false);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_security_bootstrap_vzw(void)
{
    const uint16_t instance_id = LWM2M_BOOTSTRAP_INSTANCE_ID;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, LWM2M_VZW_BOOTSTRAP_SSID);
    lwm2m_security_is_bootstrap_server_set(instance_id, true);
    lwm2m_security_hold_off_timer_set(10);
    lwm2m_security_bootstrapped_set(false);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_security_diagnostics_vzw(lwm2m_carrier_config_t * p_carrier_config)
{
    const uint16_t instance_id = 2;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_security_get_instance(instance_id);

    lwm2m_security_short_server_id_set(instance_id, LWM2M_VZW_DIAGNOSTICS_SSID);
    if (p_carrier_config->certification_mode || lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
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

    lwm2m_server_short_server_id_set(instance_id, LWM2M_VZW_BOOTSTRAP_SSID);
    lwm2m_server_client_hold_off_timer_set(instance_id, 0);

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_management_acl_vzw(void)
{
    const uint16_t instance_id = 1;
    uint16_t access[] = { rwde_access, rwde_access, rwde_access };
    uint16_t servers[] = { LWM2M_VZW_DIAGNOSTICS_SSID, LWM2M_VZW_MANAGEMENT_SSID, LWM2M_VZW_REPOSITORY_SSID };
    uint16_t owner = LWM2M_VZW_MANAGEMENT_SSID;

    lwm2m_list_t acl = {
        .p_id = servers,
        .val.p_uint16 = access,
        .len = ARRAY_SIZE(servers)
    };

    lwm2m_access_control_acl_set(LWM2M_OBJ_SERVER, instance_id, &acl);
    lwm2m_access_control_owner_set(LWM2M_OBJ_SERVER, instance_id, owner);
}

static void factory_server_management_vzw(void)
{
    // Setup the ACL.
    factory_server_management_acl_vzw();
}

static void factory_server_diagnostics_acl_vzw(void)
{
    const uint16_t instance_id = 2;
    uint16_t access[] = { rwde_access };
    uint16_t servers[] = { LWM2M_VZW_MANAGEMENT_SSID };
    uint16_t owner = LWM2M_VZW_DIAGNOSTICS_SSID;

    lwm2m_list_t acl = {
        .p_id = servers,
        .val.p_uint16 = access,
        .len = ARRAY_SIZE(servers)
    };

    lwm2m_access_control_acl_set(LWM2M_OBJ_SERVER, instance_id, &acl);
    lwm2m_access_control_owner_set(LWM2M_OBJ_SERVER, instance_id, owner);
}

static void factory_server_diagnostics_vzw(void)
{
    const uint16_t instance_id = 2;
    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);

    lwm2m_server_short_server_id_set(instance_id, LWM2M_VZW_DIAGNOSTICS_SSID);
    lwm2m_server_client_hold_off_timer_set(instance_id, 30);
    lwm2m_server_lifetime_set(instance_id, 86400);
    lwm2m_server_min_period_set(instance_id, 300);
    lwm2m_server_max_period_set(instance_id, 6000);
    lwm2m_server_notif_storing_set(instance_id, 1);
    lwm2m_server_binding_set(instance_id, "UQS", 3);

    // Setup the ACL.
    factory_server_diagnostics_acl_vzw();

    lwm2m_coap_handler_instance_delete(p_instance);
    lwm2m_coap_handler_instance_add(p_instance);
}

static void factory_server_repository_acl_vzw(void)
{
    const uint16_t instance_id = 3;
    uint16_t access[] = { rwde_access, rwde_access, rwde_access };
    uint16_t servers[] = { LWM2M_VZW_DIAGNOSTICS_SSID, LWM2M_VZW_MANAGEMENT_SSID, LWM2M_VZW_REPOSITORY_SSID };
    uint16_t owner = LWM2M_VZW_REPOSITORY_SSID;

    lwm2m_list_t acl = {
        .p_id = servers,
        .val.p_uint16 = access,
        .len = ARRAY_SIZE(servers)
    };

    lwm2m_access_control_acl_set(LWM2M_OBJ_SERVER, instance_id, &acl);
    lwm2m_access_control_owner_set(LWM2M_OBJ_SERVER, instance_id, owner);
}

static void factory_server_repository_vzw(void)
{
    // Setup the ACL.
    factory_server_repository_acl_vzw();
}

/**@brief Reset factory bootstrapped objects. */
static void factory_bootstrap_reset(uint16_t instance_id)
{
    lwm2m_security_reset(instance_id);
    lwm2m_server_reset(instance_id);

    // Reset VzW specific values
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

void lwm2m_factory_bootstrap_init(lwm2m_carrier_config_t * p_carrier_config)
{
    // Initialize all instances except Bootstrap server
    for (int i = 1; i < 1+LWM2M_MAX_SERVERS; i++)
    {
        factory_bootstrap_reset(i);
    }

    if (operator_is_vzw(true))
    {
        factory_security_diagnostics_vzw(p_carrier_config);
        factory_server_management_vzw();
        factory_server_diagnostics_vzw();
        factory_server_repository_vzw();
    }

    lwm2m_storage_security_store();
    lwm2m_storage_server_store();
    lwm2m_storage_access_control_store();
}

bool lwm2m_factory_bootstrap_update(lwm2m_carrier_config_t * p_carrier_config, bool application_psk_set)
{
    const char * bootstrap_uri = NULL;
    bool   settings_changed = false;

    if (p_carrier_config->bootstrap_uri)
    {
        LWM2M_INF("Setting custom bootstrap: %s", lwm2m_os_log_strdup(p_carrier_config->bootstrap_uri));
        bootstrap_uri = p_carrier_config->bootstrap_uri;
    }
    else if (operator_is_vzw(true))
    {
        LWM2M_INF("Setting VzW bootstrap");
        if (p_carrier_config->certification_mode || lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
            // Carrier check is disabled, connect to test servers
            bootstrap_uri = BOOTSTRAP_URI_VZW_TEST;
        } else {
            // Carrier check is enabled, connect to live servers
            bootstrap_uri = BOOTSTRAP_URI_VZW;
        }
    }
    else if (operator_is_att(true))
    {
        LWM2M_INF("Setting AT&T bootstrap");
        if (p_carrier_config->certification_mode || lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
            // Carrier check is disabled, connect to test servers
            bootstrap_uri = BOOTSTRAP_URI_ATT_TEST;
        } else {
            // Carrier check is enabled, connect to live servers
            bootstrap_uri = BOOTSTRAP_URI_ATT;
        }
    }
    else
    {
        bootstrap_uri = CONFIG_NRF_LWM2M_CARRIER_BOOTSTRAP_URI;
    }

    // Never replace PSK set by application in lwm2m_carrier_init()
    if (!application_psk_set)
    {
        if (operator_is_vzw(true))
        {
            LWM2M_INF("Using VzW bootstrap PSK");
            p_carrier_config->psk = BOOTSTRAP_SEC_PSK_VZW;
        }
        else
        {
            const char *p_debug_psk = lwm2m_debug_bootstrap_psk_get();
            if (p_debug_psk)
            {
                LWM2M_INF("Using debug bootstrap PSK");
                p_carrier_config->psk = p_debug_psk;
            }
            else if (!operator_is_att(true))
            {
                LWM2M_INF("Using Nordic bootstrap PSK");
                p_carrier_config->psk = CONFIG_NRF_LWM2M_CARRIER_BOOTSTRAP_PSK;
            }
            else
            {
                // AT&T Bootstrap PSK will be generated using AT%BSKGEN
            }
        }
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