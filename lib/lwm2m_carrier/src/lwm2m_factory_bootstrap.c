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

#if USE_CONTABO
#define BOOTSTRAP_URI_CONTABO          "coaps://vmi36865.contabo.host:5784"                  /**< Server URI to the bootstrap server when using security (DTLS). */
#define BOOTSTRAP_SEC_PSK_CONTABO      { 'n', 'o', 'r', 'd', 'i', 'c', 's', 'e', 'c', 'r', 'e', 't' }  /**< Pre-shared key used for bootstrap server in hex format. */
static char m_bootstrap_psk_contabo[] = BOOTSTRAP_SEC_PSK_CONTABO;
#endif

#define BOOTSTRAP_URI_VZW              "coaps://boot.lwm2m.vzwdm.com:5684"                   /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW            "coaps://diag.lwm2m.vzwdm.com:5684"                   /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_VZW_TEST         "coaps://ddocdpboot.do.motive.com:5684"               /**< Server URI to the bootstrap server when using security (DTLS). */
#define DIAGNOSTICS_URI_VZW_TEST       ""                                                    /**< Server URI to the diagnostics server when using security (DTLS). */

#define BOOTSTRAP_URI_ATT              "coaps://InteropBootstrap.dm.iot.att.com:5694"        /**< Server URI to the bootstrap server when using security (DTLS). */
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

static bool factory_bootstrap_init_vzw(uint16_t instance_id, uint16_t *default_access, lwm2m_instance_acl_t * p_acl)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    bool initialized = true;

    switch (instance_id)
    {
        case LWM2M_BOOTSTRAP_INSTANCE_ID:
        {
            lwm2m_security_short_server_id_set(instance_id, 100);
            lwm2m_security_is_bootstrap_server_set(instance_id, true);
            lwm2m_security_bootstrapped_set(instance_id, false);
            lwm2m_security_hold_off_timer_set(instance_id, 10);

            lwm2m_server_short_server_id_set(instance_id, 100);
            lwm2m_server_client_hold_off_timer_set(instance_id, 0);

            p_acl->access[0] = rwde_access;
            p_acl->server[0] = 102;
            break;
        }

        case 1: // Management instance
        {
            p_acl->access[0] = rwde_access;
            p_acl->server[0] = 101;
            p_acl->access[1] = rwde_access;
            p_acl->server[1] = 102;
            p_acl->access[2] = rwde_access;
            p_acl->server[2] = 1000;
            break;
        }

        case 2: // Diagnostics instance
        {
            lwm2m_security_short_server_id_set(instance_id, 101);
            if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
                lwm2m_security_server_uri_set(instance_id, DIAGNOSTICS_URI_VZW_TEST, strlen(DIAGNOSTICS_URI_VZW_TEST));
            } else {
                lwm2m_security_server_uri_set(instance_id, DIAGNOSTICS_URI_VZW, strlen(DIAGNOSTICS_URI_VZW));
            }

            lwm2m_server_short_server_id_set(instance_id, 101);
            lwm2m_server_client_hold_off_timer_set(instance_id, 30);
            lwm2m_server_lifetime_set(instance_id, 86400);
            lwm2m_server_min_period_set(instance_id, 300);
            lwm2m_server_max_period_set(instance_id, 6000);
            lwm2m_server_notif_storing_set(instance_id, 1);
            lwm2m_server_binding_set(instance_id, "UQS", 3);

            p_acl->access[0] = rwde_access;
            p_acl->server[0] = 102;
            p_acl->owner = 101;
            break;
        }

        case 3: // Repository instance
        {
            p_acl->access[0] = rwde_access;
            p_acl->server[0] = 101;
            p_acl->access[1] = rwde_access;
            p_acl->server[1] = 102;
            p_acl->access[2] = rwde_access;
            p_acl->server[2] = 1000;
            break;
        }

        default:
            initialized = false;
            break;
    }

    return initialized;
}

static bool factory_bootstrap_init_att(uint16_t instance_id, uint16_t *default_access, lwm2m_instance_acl_t * p_acl)
{
    bool initialized = true;

    switch (instance_id)
    {
        case LWM2M_BOOTSTRAP_INSTANCE_ID:
        {
            lwm2m_security_short_server_id_set(instance_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);
            lwm2m_security_is_bootstrap_server_set(instance_id, true);

            lwm2m_server_short_server_id_set(instance_id, LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID);

            *default_access = 0;
            break;
        }

        case 2: // Production server instance
        {
            p_acl->access[0] = LWM2M_ACL_RWEDO_PERM;
            p_acl->server[0] = 1;
            break;
        }

        default:
            initialized = false;
            break;
    }

    return initialized;
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

/**@brief Initialize factory bootstrapped objects. */
void lwm2m_factory_bootstrap_init(uint16_t instance_id)
{
    uint16_t default_access = LWM2M_PERMISSION_READ;
    bool initialized = false;

    lwm2m_instance_acl_t acl = {
        .owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID
    };

    factory_bootstrap_reset(instance_id);

    if (operator_is_vzw(true))
    {
        initialized = factory_bootstrap_init_vzw(instance_id, &default_access, &acl);
    }
    else if (operator_is_att(true))
    {
        initialized = factory_bootstrap_init_att(instance_id, &default_access, &acl);
    }

    lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);
    lwm2m_set_instance_acl(p_instance, default_access, &acl);

    if (initialized)
    {
        lwm2m_instance_storage_security_store(instance_id);
        lwm2m_instance_storage_server_store(instance_id);

        lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)lwm2m_security_get_instance(instance_id));
        lwm2m_coap_handler_instance_add((lwm2m_instance_t *)lwm2m_security_get_instance(instance_id));

        lwm2m_coap_handler_instance_delete((lwm2m_instance_t *)lwm2m_server_get_instance(instance_id));
        lwm2m_coap_handler_instance_add((lwm2m_instance_t *)lwm2m_server_get_instance(instance_id));
    }
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
#if USE_CONTABO
    else
    {
        bootstrap_uri = BOOTSTRAP_URI_CONTABO;
        p_carrier_config->psk = m_bootstrap_psk_contabo;
        p_carrier_config->psk_length = sizeof(m_bootstrap_psk_contabo);
    }
#endif

    uint8_t p_len = 0;
    char * p = lwm2m_security_server_uri_get(LWM2M_BOOTSTRAP_INSTANCE_ID, &p_len);
    if (bootstrap_uri && (p_len == 0 || strncmp(p, bootstrap_uri, p_len) != 0)) {
        // Initial startup (no server URI) or server URI has changed (carrier changed).
        // Clear all bootstrap settings and load factory settings.
        lwm2m_factory_bootstrap_init(LWM2M_BOOTSTRAP_INSTANCE_ID);

        lwm2m_security_server_uri_set(LWM2M_BOOTSTRAP_INSTANCE_ID, bootstrap_uri, strlen(bootstrap_uri));
        lwm2m_instance_storage_security_store(LWM2M_BOOTSTRAP_INSTANCE_ID);
        lwm2m_instance_storage_server_store(LWM2M_BOOTSTRAP_INSTANCE_ID);

        settings_changed = true;
    }

    return settings_changed;
}