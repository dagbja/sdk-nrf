/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <nrf_socket.h>

#include <lwm2m_api.h>
#include <lwm2m_acl.h>
#include <lwm2m_remote.h>
#include <operator_check.h>

/**@brief Helper function to get the access from an instance and a remote. */
uint32_t lwm2m_access_remote_get(uint16_t            * p_access,
                                 lwm2m_instance_t    * p_instance,
                                 struct nrf_sockaddr * p_remote)
{
    uint16_t short_server_id;
    uint32_t err_code = lwm2m_remote_short_server_id_find(&short_server_id, p_remote);

    if (err_code == 0)
    {
        err_code = lwm2m_acl_permissions_check(p_access, p_instance, short_server_id);

        // If we can't find the permission we return defaults.
        if (err_code != 0)
        {
            err_code = lwm2m_acl_permissions_check(p_access,
                                                p_instance,
                                                LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);
        }
    }

    if ((*p_access & LWM2M_PERMISSION_READ) > 0)
    {
        // Observe and discover is allowed if READ is allowed.
        *p_access = (*p_access | LWM2M_OPERATION_CODE_DISCOVER | LWM2M_OPERATION_CODE_OBSERVE);
    }

    return err_code;
}

void lwm2m_set_instance_acl(lwm2m_instance_t * p_instance,
                            uint16_t default_access,
                            lwm2m_instance_acl_t * p_acl)
{
    // Reset ACL on the instance.
    (void)lwm2m_acl_permissions_reset(p_instance, p_acl->owner);

    // Set default access.
    (void)lwm2m_acl_permissions_add(p_instance,
                                    default_access,
                                    LWM2M_ACL_DEFAULT_SHORT_SERVER_ID);

    for (uint32_t i = 0; i < ARRAY_SIZE(p_acl->server); i++)
    {
        if (p_acl->server[i] != 0)
        {
            // Set server access.
            (void)lwm2m_acl_permissions_add(p_instance, p_acl->access[i], p_acl->server[i]);
        }
    }
}

void lwm2m_set_carrier_acl(lwm2m_instance_t * p_instance)
{
    uint16_t rwde_access = (LWM2M_PERMISSION_READ | LWM2M_PERMISSION_WRITE |
                            LWM2M_PERMISSION_DELETE | LWM2M_PERMISSION_EXECUTE);

    lwm2m_instance_acl_t acl = {
        .owner = LWM2M_ACL_BOOTSTRAP_SHORT_SERVER_ID
    };

    if (operator_is_vzw(true))
    {
        acl.access[0] = rwde_access;
        acl.server[0] = 101;
        acl.access[1] = rwde_access;
        acl.server[1] = 102;
        acl.access[2] = rwde_access;
        acl.server[2] = 1000;
    }
    else if (operator_is_att(true))
    {
        acl.access[0] = rwde_access;
        acl.server[0] = 1;
    }
    else
    {
        // TODO: Remove when fixing ACL
        acl.access[0] = rwde_access;
        acl.server[0] = 123;
    }

    lwm2m_set_instance_acl(p_instance, LWM2M_PERMISSION_READ, &acl);
}
