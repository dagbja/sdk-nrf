/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <net/socket.h>

#include <lwm2m_api.h>
#include <lwm2m_acl.h>
#include <lwm2m_remote.h>

/**@brief Helper function to get the access from an instance and a remote. */
uint32_t common_lwm2m_access_remote_get(uint16_t         * p_access,
                                     lwm2m_instance_t * p_instance,
                                     struct sockaddr  * p_remote)
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

    return err_code;
}