/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#if CONFIG_SHELL

#define LWM2M_LOG_MODULE_NAME lwm2m_shell

#include <shell/shell.h>
#include <fcntl.h>

#include <lwm2m_instance_storage.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_device.h>
#include <lwm2m_retry_delay.h>
#include <at_interface.h>
#include <sms_receive.h>
#include <lwm2m_vzw_main.h>


static int cmd_at_command(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"AT command\"", argv[0]);
        return 0;
    }

    at_send_command(argv[1], true);

    return 0;
}


static int cmd_config_clear(const struct shell *shell, size_t argc, char **argv)
{
    app_bootstrap_reset();
    shell_print(shell, "Deleted all bootstrapped values");

    return 0;
}


static int cmd_config_print(const struct shell *shell, size_t argc, char **argv)
{
    uint8_t uri_len = 0;
    // Buffer for the URI with null terminator
    char terminated_uri[128];

    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++) {
        if (lwm2m_server_short_server_id_get(i)) {
            lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(i);
            char * server_uri = lwm2m_security_server_uri_get(i, &uri_len);
            if (uri_len > 127) {
                uri_len = 127;
            }
            memcpy(terminated_uri, server_uri, uri_len);
            terminated_uri[uri_len] = '\0';

            shell_print(shell, "Instance %d", i);
            shell_print(shell, "  Short Server ID  %d", lwm2m_server_short_server_id_get(i));
            shell_print(shell, "  Server URI       %s", terminated_uri);
            shell_print(shell, "  Lifetime         %lld", lwm2m_server_lifetime_get(i));
            shell_print(shell, "  Owner            %d", p_instance->acl.owner);
        }
    }

    return 0;
}


static int cmd_config_uri(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_print(shell, "%s <instance> <URI>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);
    char *uri = argv[2];
    size_t uri_len = strlen(uri);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (uri_len > SECURITY_SERVER_URI_SIZE_MAX)
    {
        shell_print(shell, "maximum URI length is %d", SECURITY_SERVER_URI_SIZE_MAX);
        return 0;
    }

    lwm2m_security_server_uri_set(instance_id, uri, strlen(uri));
    lwm2m_instance_storage_security_store(instance_id);

    shell_print(shell, "Set URI %d: %s", instance_id, uri);

    return 0;
}


static int cmd_config_lifetime(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_print(shell, "%s <instance> <seconds>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);
    time_t lifetime = (time_t) atoi(argv[2]);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (lifetime != lwm2m_server_lifetime_get(instance_id)) {
        if (instance_id == 1 || instance_id == 3) {
            // Lifetime changed, send update server
            app_request_server_update(instance_id);
        }

        lwm2m_server_lifetime_set(instance_id, lifetime);
        lwm2m_instance_storage_server_store(instance_id);

        shell_print(shell, "Set lifetime %d: %d", instance_id, lifetime);
    }

    return 0;
}


static int cmd_debug_print(const struct shell *shell, size_t argc, char **argv)
{
    const char * p_debug_msisdn = app_debug_msisdn_get();

    shell_print(shell, "Debug configuration");
    shell_print(shell, "  IMEI           %s", app_imei_get());

    if (p_debug_msisdn && p_debug_msisdn[0]) {
        shell_print(shell, "  MSISDN         %s (static)", p_debug_msisdn);
    } else {
        shell_print(shell, "  MSISDN         %s", app_msisdn_get());
    }

    uint32_t iccid_len;
    char * p_iccid = lwm2m_device_get_sim_iccid(&iccid_len);
    char iccid[21];
    if (p_iccid) {
        memcpy(iccid, p_iccid, 20);
        iccid[iccid_len] = 0;
    } else {
        iccid[0] = 0;
    }

    shell_print(shell, "  SIM ICCID      %s", iccid);
    shell_print(shell, "  Logging        %s", app_debug_modem_logging_get());
    shell_print(shell, "  Disable PSM    %s", app_debug_flag_is_set(DEBUG_FLAG_DISABLE_PSM) ? "Yes" : "No");
    shell_print(shell, "  PDN Support    %s", app_debug_flag_is_set(DEBUG_FLAG_PDN_SUPPORT) ? "Yes" : "No");
    shell_print(shell, "  SMS Support    %s", app_debug_flag_is_set(DEBUG_FLAG_SMS_SUPPORT) ? "Yes" : "No");

    if (app_debug_flag_is_set(DEBUG_FLAG_SMS_SUPPORT)) {
        shell_print(shell, "  SMS Counter    %u", sms_receive_counter());
    }

    return 0;
}


static int cmd_debug_reset(const struct shell *shell, size_t argc, char **argv)
{
    app_debug_clear();

    return 0;
}


static int cmd_debug_msisdn(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s MSISDN", argv[0]);
        return 0;
    }

    char *msisdn = argv[1];
    size_t msisdn_len = strlen(msisdn);

    if (msisdn_len != 0 && msisdn_len != 10) {
        shell_print(shell, "length of MSISDN must be 10");
        return 0;
    }

    app_debug_msisdn_set(msisdn);

    if (msisdn_len) {
        shell_print(shell, "Set static MSISDN: %s", msisdn);
    } else {
        shell_print(shell, "Removed static MSISDN");
    }

    return 0;
}


static int cmd_debug_logging(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = fidoless generic");
        shell_print(shell, " 2 = fido");
        shell_print(shell, " 3 = fidoless \"lwm2m\"");
        shell_print(shell, " 4 = fidoless IP only");
        return 0;
    }

    char *logging = argv[1];
    size_t logging_len = strlen(logging);

    if (logging_len != 1 && logging_len != 64) {
        shell_print(shell, "invalid logging value");
        return 0;
    }

    app_debug_modem_logging_set(logging);
    app_debug_modem_logging_enable();

    shell_print(shell, "Set logging value: %s", logging);

    return 0;
}


static int cmd_debug_disable_psm(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = don't disable");
        shell_print(shell, " 1 = disable");
        return 0;
    }

    int disable_psm = atoi(argv[1]);

    if (disable_psm != 0 && disable_psm != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (disable_psm) {
        app_debug_flag_set(DEBUG_FLAG_DISABLE_PSM);
    } else {
        app_debug_flag_clear(DEBUG_FLAG_DISABLE_PSM);
    }

    shell_print(shell, "Set disable PSM: %d", disable_psm);

    return 0;
}

static int cmd_debug_pdn_support(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int pdn_support = atoi(argv[1]);

    if (pdn_support != 0 && pdn_support != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (pdn_support) {
        app_debug_flag_set(DEBUG_FLAG_PDN_SUPPORT);
    } else {
        app_debug_flag_clear(DEBUG_FLAG_PDN_SUPPORT);
    }

    shell_print(shell, "Set PDN support: %d", pdn_support);

    return 0;
}

static int cmd_debug_sms_support(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int sms_support = atoi(argv[1]);

    if (sms_support != 0 && sms_support != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (sms_support) {
        app_debug_flag_set(DEBUG_FLAG_SMS_SUPPORT);
        sms_receive_thread_start();
    } else {
        app_debug_flag_clear(DEBUG_FLAG_SMS_SUPPORT);
    }

    shell_print(shell, "Set SMS support: %d", sms_support);

    return 0;
}

static int cmd_lwm2m_register(const struct shell *shell, size_t argc, char **argv)
{
    if (app_state_get() == APP_STATE_IP_INTERFACE_UP) {
        if (lwm2m_security_bootstrapped_get(0)) {
            app_state_set(APP_STATE_SERVER_CONNECT);
        } else {
            app_state_set(APP_STATE_BS_CONNECT);
        }
    } else if (app_state_get() == APP_STATE_SERVER_REGISTERED) {
        shell_print(shell, "Already registered");
    } else {
        shell_print(shell, "Wrong state for registration");
    }

    return 0;
}


static int cmd_lwm2m_update(const struct shell *shell, size_t argc, char **argv)
{
    uint16_t instance_id = 1;

    if (argc == 2) {
        instance_id = atoi(argv[1]);

        if (instance_id != 1 && instance_id != 3)
        {
            shell_print(shell, "instance must be 1 or 3");
            return 0;
        }
    }

    if (app_state_get() == APP_STATE_SERVER_REGISTERED) {
        app_request_server_update(instance_id);
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_deregister(const struct shell *shell, size_t argc, char **argv)
{
    if (app_state_get() == APP_STATE_SERVER_REGISTERED) {
        app_state_set(APP_STATE_SERVER_DEREGISTER);
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_status(const struct shell *shell, size_t argc, char **argv)
{
    char ip_version[] = "IPvX";
    ip_version[3] = (app_family_type_get(app_server_instance()) == AF_INET6) ? '6' : '4';
    int32_t retry_delay;

    if (app_did_bootstrap()) {
        shell_print(shell, "Bootstrap completed [%s]", (app_family_type_get(0) == AF_INET6) ? "IPv6" : "IPv4");
    }

    for (int i = 1; i < (1+LWM2M_MAX_SERVERS); i++) {
        uint8_t uri_len = 0;
        (void)lwm2m_security_server_uri_get(i, &uri_len);
        if ((uri_len > 0) && lwm2m_server_registered_get(i)) {
            shell_print(shell, "Server %d registered [%s]", i, (app_family_type_get(i) == AF_INET6) ? "IPv6" : "IPv4");
        }
    }

    switch(app_state_get())
    {
        case APP_STATE_IDLE:
            shell_print(shell, "Idle");
            break;
        case APP_STATE_IP_INTERFACE_UP:
            shell_print(shell, "Disconnected");
            break;
        case APP_STATE_BS_CONNECT:
            shell_print(shell, "Bootstrap connecting [%s]", ip_version);
            break;
        case APP_STATE_BS_CONNECT_WAIT:
            shell_print(shell, "Bootstrap connect wait [%s]", ip_version);
            break;
        case APP_STATE_BS_CONNECT_RETRY_WAIT:
            retry_delay = lwm2m_retry_delay_get(0, false);
            if (retry_delay != -1) {
                int32_t delay = app_state_update_delay() / 1000;
                shell_print(shell, "Bootstrap connect delay (%d minutes - %d seconds left) [%s]",
                            retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Bootstrap connect timed wait [%s]", ip_version);
            }
            break;
        case APP_STATE_BS_CONNECTED:
            shell_print(shell, "Bootstrap connected [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAP_REQUESTED:
            shell_print(shell, "Bootstrap requested [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAP_WAIT:
            retry_delay = lwm2m_retry_delay_get(0, false);
            if (retry_delay != -1) {
                int32_t delay = app_state_update_delay() / 1000;
                shell_print(shell, "Bootstrap delay (%d minutes - %d seconds left) [%s]",
                            retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Bootstrap wait [%s]", ip_version);
            }
            break;
        case APP_STATE_BOOTSTRAPPING:
            shell_print(shell, "Bootstrapping [%s]", ip_version);
            break;
        case APP_STATE_BOOTSTRAPPED:
            shell_print(shell, "Bootstrapped [%s]", ip_version);
            break;
        case APP_STATE_SERVER_CONNECT:
            shell_print(shell, "Server %d connecting [%s]", app_server_instance(), ip_version);
            break;
        case APP_STATE_SERVER_CONNECT_WAIT:
            shell_print(shell, "Server %d connect wait [%s]", app_server_instance(), ip_version);
            break;
        case APP_STATE_SERVER_CONNECT_RETRY_WAIT:
            retry_delay = lwm2m_retry_delay_get(app_server_instance(), false);
            if (retry_delay != -1) {
                int32_t delay = app_state_update_delay() / 1000;
                shell_print(shell, "Server %d connect delay (%d minutes - %d seconds left) [%s]",
                            app_server_instance(), retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Server %d connect timed wait [%s]", app_server_instance(), ip_version);
            }
            break;
        case APP_STATE_SERVER_CONNECTED:
            shell_print(shell, "Server %d connected [%s]", app_server_instance(), ip_version);
            break;
        case APP_STATE_SERVER_REGISTER_WAIT:
            retry_delay = lwm2m_retry_delay_get(app_server_instance(), false);
            if (retry_delay != -1) {
                int32_t delay = app_state_update_delay() / 1000;
                shell_print(shell, "Server %d register delay (%d minutes - %d seconds left) [%s]",
                            app_server_instance(), retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Server %d register wait [%s]", app_server_instance(), ip_version);
            }
            break;
        case APP_STATE_SERVER_REGISTERED:
            // Already printed above
            break;
        case APP_STATE_SERVER_DEREGISTER:
            shell_print(shell, "Server %d deregister", app_server_instance());
            break;
        case APP_STATE_SERVER_DEREGISTERING:
            shell_print(shell, "Server %d deregistering", app_server_instance());
            break;
        case APP_STATE_DISCONNECT:
            shell_print(shell, "Disconnect");
            break;
        case APP_STATE_SHUTDOWN:
            shell_print(shell, "Shutdown");
            break;
        default:
            shell_print(shell, "Unknown state: %d", app_state_get());
            break;
    };

    return 0;
}


static int cmd_factory_reset(const struct shell *shell, size_t argc, char **argv)
{
    app_factory_reset();
    app_system_reset();

    return 0;
}


static int cmd_reboot(const struct shell *shell, size_t argc, char **argv)
{
    app_system_reset();

    return 0;
}


static int cmd_shutdown(const struct shell *shell, size_t argc, char **argv)
{
    app_system_shutdown();

    return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_config,
    SHELL_CMD(print, NULL, "Print configuration", cmd_config_print),
    SHELL_CMD(clear, NULL, "Clear bootstrapped values", cmd_config_clear),
    SHELL_CMD(uri, NULL, "Set URI", cmd_config_uri),
    SHELL_CMD(lifetime, NULL, "Set lifetime", cmd_config_lifetime),
    SHELL_CMD(factory_reset, NULL, "Factory reset", cmd_factory_reset),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_debug,
    SHELL_CMD(print, NULL, "Print configuration", cmd_debug_print),
    SHELL_CMD(reset, NULL, "Reset configuration", cmd_debug_reset),
    SHELL_CMD(msisdn, NULL, "Set static MSISDN", cmd_debug_msisdn),
    SHELL_CMD(logging, NULL, "Set logging value", cmd_debug_logging),
    SHELL_CMD(disable_psm, NULL, "Disable PSM", cmd_debug_disable_psm),
    SHELL_CMD(pdn_support, NULL, "Set PDN Support", cmd_debug_pdn_support),
    SHELL_CMD(sms_support, NULL, "Set SMS Support", cmd_debug_sms_support),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_lwm2m,
    SHELL_CMD(status, NULL, "Application status", cmd_lwm2m_status),
    SHELL_CMD(register, NULL, "Register server", cmd_lwm2m_register),
    SHELL_CMD(update, NULL, "Update server", cmd_lwm2m_update),
    SHELL_CMD(deregister, NULL, "Deregister server", cmd_lwm2m_deregister),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_CMD_REGISTER(at, NULL, "Send AT command", cmd_at_command);
SHELL_CMD_REGISTER(config, &sub_config, "Instance configuration", NULL);
SHELL_CMD_REGISTER(debug, &sub_debug, "Debug configuration", NULL);
SHELL_CMD_REGISTER(lwm2m, &sub_lwm2m, "LwM2M operations", NULL);
SHELL_CMD_REGISTER(reboot, NULL, "Reboot", cmd_reboot);
SHELL_CMD_REGISTER(shutdown, NULL, "Shutdown", cmd_shutdown);

#endif // CONFIG_SHELL
