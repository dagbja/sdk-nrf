/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#if CONFIG_SHELL

#include <stdio.h>
#include <stdlib.h>

#include <shell/shell.h>
#include <fcntl.h>

#include <lwm2m_api.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_device.h>
#include <lwm2m_retry_delay.h>
#include <lwm2m_instance_storage.h>
#include <at_interface.h>
#include <sms_receive.h>
#include <lwm2m_vzw_main.h>
#include <modem_logging.h>
#include <lwm2m_carrier.h>
#include <lwm2m_objects.h>

static int cmd_at_command(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"AT command\"", argv[0]);
        return 0;
    }

    (void)modem_at_write(argv[1], true);

    return 0;
}


static int cmd_config_clear(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_bootstrap_clear();
    shell_print(shell, "Cleared bootstrapped");

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
            shell_print(shell, "  Lifetime         %ld", lwm2m_server_lifetime_get(i));
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

    lwm2m_security_server_uri_set(instance_id, uri, uri_len);
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
    lwm2m_time_t lifetime = (lwm2m_time_t) atoi(argv[2]);

    if (instance_id < 0 || instance_id >= (1+LWM2M_MAX_SERVERS))
    {
        shell_print(shell, "instance must be between 0 and %d", LWM2M_MAX_SERVERS);
        return 0;
    }

    if (lifetime != lwm2m_server_lifetime_get(instance_id)) {
        // Lifetime changed, send update server
        lwm2m_request_server_update(instance_id, false);
        lwm2m_server_lifetime_set(instance_id, lifetime);
        lwm2m_instance_storage_server_store(instance_id);

        shell_print(shell, "Set lifetime %d: %d", instance_id, lifetime);
    }

    return 0;
}


static int cmd_debug_print(const struct shell *shell, size_t argc, char **argv)
{
    shell_print(shell, "Debug configuration");
    shell_print(shell, "  IMEI           %s", lwm2m_imei_get());
    shell_print(shell, "  MSISDN         %s", lwm2m_msisdn_get());

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
    shell_print(shell, "  Logging        %s", modem_logging_get());
    shell_print(shell, "  Carrier check  %s", lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK) ? "No" : "Yes");
    shell_print(shell, "  IPv6 enabled   %s", lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_IPv6) ? "No" : "Yes");
    shell_print(shell, "  IP fallback    %s", lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_FALLBACK) ? "No" : "Yes");
    shell_print(shell, "  CON interval   %d seconds", (int32_t)lwm2m_coap_con_interval_get());
    shell_print(shell, "  SMS Counter    %u", lwm2m_sms_receive_counter());
    shell_print(shell, "  Network status %u", lwm2m_net_reg_stat_get());

    return 0;
}


static int cmd_debug_reset(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_debug_reset();

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

    modem_logging_set(logging);
    modem_logging_enable();

    shell_print(shell, "Set logging value: %s", logging);
    shell_print(shell, "Remember to do 'reboot' to store this value permanent!");

    return 0;
}


static int cmd_debug_carrier_check(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int carrier_check = atoi(argv[1]);

    if (carrier_check != 0 && carrier_check != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (carrier_check) {
        lwm2m_debug_clear(LWM2M_DEBUG_DISABLE_CARRIER_CHECK);
    } else {
        lwm2m_debug_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK);
    }

    shell_print(shell, "Set carrier check: %d", carrier_check);

    return 0;
}

static int cmd_debug_set_net_reg_stat(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = offline");
        shell_print(shell, " 1 = home");
        shell_print(shell, " 2 = search");
        shell_print(shell, " 5 = roaming");
        return 0;
    }

    int net_reg_stat = atoi(argv[1]);

    if (net_reg_stat < 0 || net_reg_stat > 5) {
        shell_print(shell, "invalid value, must be between 0 and 5");
        return 0;
    }

    lwm2m_net_reg_stat_cb(net_reg_stat);

    shell_print(shell, "Set network registration status: %d", net_reg_stat);

    return 0;
}

static int cmd_debug_ipv6_enabled(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int enable_ipv6 = atoi(argv[1]);

    if (enable_ipv6 != 0 && enable_ipv6 != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (enable_ipv6) {
        lwm2m_debug_clear(LWM2M_DEBUG_DISABLE_IPv6);
    } else {
        lwm2m_debug_set(LWM2M_DEBUG_DISABLE_IPv6);
    }

    shell_print(shell, "Set IPv6 enabled: %d", enable_ipv6);

    return 0;
}

static int cmd_debug_fallback_disabled(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int enable_fallback = atoi(argv[1]);

    if (enable_fallback != 0 && enable_fallback != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (enable_fallback) {
        lwm2m_debug_clear(LWM2M_DEBUG_DISABLE_FALLBACK);
    } else {
        lwm2m_debug_set(LWM2M_DEBUG_DISABLE_FALLBACK);
    }

    shell_print(shell, "Set IP fallback: %d", enable_fallback);

    return 0;
}

static int cmd_debug_con_interval(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <seconds>", argv[0]);
        return 0;
    }

    int con_interval = atoi(argv[1]);

    if (con_interval < 0 || con_interval > 86400) {
        shell_print(shell, "invalid value, must be between 0 and 86400 (24 hours)");
        return 0;
    }

    lwm2m_coap_con_interval_set(con_interval);
    lwm2m_debug_con_interval_set(con_interval);

    shell_print(shell, "Set CoAP CON interval: %d seconds", con_interval);

    return 0;
}

static int cmd_lwm2m_register(const struct shell *shell, size_t argc, char **argv)
{
    if (lwm2m_state_get() == LWM2M_STATE_DISCONNECTED) {
        lwm2m_request_connect();
    } else if (lwm2m_state_get() == LWM2M_STATE_IDLE) {
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

        if (instance_id < 1 || instance_id >= (1+LWM2M_MAX_SERVERS))
        {
            shell_print(shell, "instance must be between 1 and %d", LWM2M_MAX_SERVERS);
            return 0;
        }
    }

    if (lwm2m_state_get() == LWM2M_STATE_IDLE) {
        lwm2m_request_server_update(instance_id, false);
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_deregister(const struct shell *shell, size_t argc, char **argv)
{
    if (lwm2m_state_get() == LWM2M_STATE_IDLE) {
        lwm2m_request_deregister();
    } else {
        shell_print(shell, "Not registered");
    }

    return 0;
}


static int cmd_lwm2m_disconnect(const struct shell *shell, size_t argc, char **argv)
{
    if (lwm2m_state_get() != LWM2M_STATE_DISCONNECTED) {
        lwm2m_request_disconnect();
    } else {
        shell_print(shell, "Not connected");
    }

    return 0;
}


static int cmd_lwm2m_status(const struct shell *shell, size_t argc, char **argv)
{
    char ip_version[] = "IPvX";
    ip_version[3] = (lwm2m_family_type_get(lwm2m_server_instance()) == NRF_AF_INET6) ? '6' : '4';
    int32_t retry_delay;

    if (lwm2m_did_bootstrap()) {
        shell_print(shell, "Bootstrap completed [%s]", (lwm2m_family_type_get(0) == NRF_AF_INET6) ? "IPv6" : "IPv4");
    }

    for (int i = 1; i < (1+LWM2M_MAX_SERVERS); i++) {
        uint8_t uri_len = 0;
        (void)lwm2m_security_server_uri_get(i, &uri_len);
        if ((uri_len > 0) && lwm2m_server_registered_get(i)) {
            shell_print(shell, "Server %d registered [%s]", i, (lwm2m_family_type_get(i) == NRF_AF_INET6) ? "IPv6" : "IPv4");
        }
    }

    switch(lwm2m_state_get())
    {
        case LWM2M_STATE_BOOTING:
            shell_print(shell, "Initializing");
            break;
        case LWM2M_STATE_IDLE:
            // Already printed above
            break;
        case LWM2M_STATE_REQUEST_CONNECT:
            shell_print(shell, "Request connect");
            break;
        case LWM2M_STATE_BS_HOLD_OFF:
            shell_print(shell, "Bootstrap hold off");
            break;
        case LWM2M_STATE_BS_CONNECT:
            shell_print(shell, "Bootstrap connecting [%s]", ip_version);
            break;
        case LWM2M_STATE_BS_CONNECT_WAIT:
            shell_print(shell, "Bootstrap connect wait [%s]", ip_version);
            break;
        case LWM2M_STATE_BS_CONNECT_RETRY_WAIT:
            retry_delay = lwm2m_retry_delay_get(0, false);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / 1000;
                shell_print(shell, "Bootstrap connect delay (%d minutes - %d seconds left) [%s]",
                            retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Bootstrap connect timed wait [%s]", ip_version);
            }
            break;
        case LWM2M_STATE_BS_CONNECTED:
            shell_print(shell, "Bootstrap connected [%s]", ip_version);
            break;
        case LWM2M_STATE_BOOTSTRAP_REQUESTED:
            shell_print(shell, "Bootstrap requested [%s]", ip_version);
            break;
        case LWM2M_STATE_BOOTSTRAP_WAIT:
            retry_delay = lwm2m_retry_delay_get(0, false);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / 1000;
                shell_print(shell, "Bootstrap delay (%d minutes - %d seconds left) [%s]",
                            retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Bootstrap wait [%s]", ip_version);
            }
            break;
        case LWM2M_STATE_BOOTSTRAPPING:
            shell_print(shell, "Bootstrapping [%s]", ip_version);
            break;
        case LWM2M_STATE_CLIENT_HOLD_OFF:
            shell_print(shell, "Client hold off (server %d)", lwm2m_server_instance());
            break;
        case LWM2M_STATE_SERVER_CONNECT:
            shell_print(shell, "Server %d connecting [%s]", lwm2m_server_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_CONNECT_WAIT:
            shell_print(shell, "Server %d connect wait [%s]", lwm2m_server_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT:
            retry_delay = lwm2m_retry_delay_get(lwm2m_server_instance(), false);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / 1000;
                shell_print(shell, "Server %d connect delay (%d minutes - %d seconds left) [%s]",
                            lwm2m_server_instance(), retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Server %d connect timed wait [%s]", lwm2m_server_instance(), ip_version);
            }
            break;
        case LWM2M_STATE_SERVER_CONNECTED:
            shell_print(shell, "Server %d connected [%s]", lwm2m_server_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_REGISTER_WAIT:
            retry_delay = lwm2m_retry_delay_get(lwm2m_server_instance(), false);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / 1000;
                shell_print(shell, "Server %d register delay (%d minutes - %d seconds left) [%s]",
                            lwm2m_server_instance(), retry_delay / 60, delay, ip_version);
            } else {
                shell_print(shell, "Server %d register wait [%s]", lwm2m_server_instance(), ip_version);
            }
            break;
        case LWM2M_STATE_SERVER_DEREGISTER:
            shell_print(shell, "Server %d deregister", lwm2m_server_instance());
            break;
        case LWM2M_STATE_SERVER_DEREGISTERING:
            shell_print(shell, "Server %d deregistering", lwm2m_server_instance());
            break;
        case LWM2M_STATE_REQUEST_DISCONNECT:
            shell_print(shell, "Request disconnect");
            break;
        case LWM2M_STATE_DISCONNECTED:
            shell_print(shell, "Disconnected");
            break;
        case LWM2M_STATE_SHUTDOWN:
            shell_print(shell, "Shutdown");
            break;
        default:
            shell_print(shell, "Unknown state: %d", lwm2m_state_get());
            break;
    };

    return 0;
}


static int cmd_factory_reset(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_factory_reset();
    lwm2m_request_reset();

    return 0;
}


static int cmd_reboot(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_system_reset();

    return 0;
}


static int cmd_shutdown(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_system_shutdown();

    return 0;
}


static int cmd_device_battery_level_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "battery_level <battery level %>");
        return 0;
    }

    switch(lwm2m_carrier_battery_level_set(atoi(argv[1])))
    {
        case 0:
            shell_print(shell, "Battery level updated successfully");
            break;
        case -EINVAL:
            shell_print(shell, "Invalid value: %d", atoi(argv[1]));
            break;
        case -ENODEV:
            shell_print(shell, "No internal battery detected");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_type_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "device_type <device type>");
        return 0;
    }

    switch(lwm2m_carrier_device_type_set(argv[1]))
    {
        case 0:
            shell_print(shell, "Device type set successfully");
            break;
        case -ENOMEM:
            shell_print(shell, "Memory allocation failure");
            break;
        case -EINVAL:
            shell_print(shell, "String cannot be NULL or empty");
            break;
        case -E2BIG:
            shell_print(shell, "Input string too long");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_voltage_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_print(shell, "voltage_measurements <power source identifier> <voltage in mV>");
        return 0;
    }

    uint8_t power_source = (uint8_t)atoi(argv[1]);
    int32_t voltage = atoi(argv[2]);

    switch(lwm2m_carrier_power_source_voltage_set(power_source, voltage))
    {
        case 0:
            shell_print(shell, "Voltage measurement updated successfully");
            break;
        case -ENODEV:
            shell_print(shell, "Power source not detected");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported power source type");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_current_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_print(shell, "current_measurements <power source identifier> <current in mA>");
        return 0;
    }

    uint8_t power_source = (uint8_t)atoi(argv[1]);
    int32_t current = atoi(argv[2]);

    switch(lwm2m_carrier_power_source_current_set(power_source, current))
    {
        case 0:
            shell_print(shell, "Current measurements updated successfully");
            break;
        case -ENODEV:
            shell_print(shell, "Power source not detected");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported power source type");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_battery_status_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, " 0 = Normal");
        shell_print(shell, " 1 = Charging");
        shell_print(shell, " 2 = Charge complete");
        shell_print(shell, " 3 = Damaged");
        shell_print(shell, " 4 = Low battery");
        shell_print(shell, " 5 = Not installed");
        shell_print(shell, " 6 = Unknown");
        return 0;
    }

    int32_t status = (int32_t)atoi(argv[1]);

    switch(lwm2m_carrier_battery_status_set(status))
    {
        case 0:
            shell_print(shell, "Battery status updated successfully");
            break;
        case -ENODEV:
            shell_print(shell, "No internal battery detected");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported battery status");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_memory_total_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "memory_total <total memory in kB>");
        return 0;
    }

    switch(lwm2m_carrier_memory_total_set(strtoul(argv[1], NULL, 10)))
    {
        case 0:
            shell_print(shell, "Total amount of storage space set successfully");
            break;
        case -EINVAL:
            shell_print(shell, "Reported value is negative or bigger than INT32_MAX");
            break;
        default:
            break;
    }

    return 0;
}

static int m_mem_free = 0;

int lwm2m_device_memory_free_read(void)
{
    return m_mem_free;
}

static int cmd_device_memory_free_write(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "memory_free <available memory in kB>");
        return 0;
    }

    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);

    if (atoi(argv[1]) < 0)
    {
        shell_print(shell, "Memory free cannot be negative");
        return 0;
    }
    else if (atoi(argv[1]) > device_obj_instance->memory_total)
    {
        shell_print(shell, "Memory free cannot be larger than memory total");
        return 0;
    }
    else
    {
        m_mem_free = atoi(argv[1]);
        shell_print(shell, "Estimated amount of storage space updated successfully");
    }

    return 0;
}


static int cmd_device_power_sources_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, " 0 = DC");
        shell_print(shell, " 1 = Internal battery");
        shell_print(shell, " 2 = External battery");
        shell_print(shell, " 4 = Ethernet");
        shell_print(shell, " 5 = USB");
        shell_print(shell, " 6 = AC");
        shell_print(shell, " 7 = Solar");
        return 0;
    }

    uint8_t power_source_count = argc - 1;
    uint8_t power_sources[power_source_count];

    for (int i = 0; i < power_source_count; i++)
    {
        power_sources[i] = (uint8_t)atoi(argv[i + 1]);
    }

    switch(lwm2m_carrier_avail_power_sources_set(power_sources, power_source_count))
    {
        case 0:
            shell_print(shell, "Available power sources set successfully");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported power source");
            break;
        case -E2BIG:
            shell_print(shell, "Unsupported number of power sources");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_software_version_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "software_version <software version>");
        return 0;
    }

    switch(lwm2m_carrier_software_version_set(argv[1]))
    {
        case 0:
            shell_print(shell, "Software version set successfully");
            break;
        case -ENOMEM:
            shell_print(shell, "Memory allocation failure");
            break;
        case -EINVAL:
            shell_print(shell, "String cannot be NULL or empty");
            break;
        case -E2BIG:
            shell_print(shell, "Input string too long");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_hardware_version_set(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "hardware_version <hardware version>");
        return 0;
    }

    switch(lwm2m_carrier_hardware_version_set(argv[1]))
    {
        case 0:
            shell_print(shell, "Hardware version set successfully");
            break;
        case -ENOMEM:
            shell_print(shell, "Memory allocation failure");
            break;
        case -EINVAL:
            shell_print(shell, "String cannot be NULL or empty");
            break;
        case -E2BIG:
            shell_print(shell, "Input string too long");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    };

    return 0;
}


static int cmd_device_error_code_add(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, " 0 = No error");
        shell_print(shell, " 1 = Low charge");
        shell_print(shell, " 2 = External supply off");
        shell_print(shell, " 3 = GPS failure");
        shell_print(shell, " 4 = Low signal");
        shell_print(shell, " 5 = Out of memory");
        shell_print(shell, " 6 = SMS failure");
        shell_print(shell, " 7 = IP connectivity failure");
        shell_print(shell, " 8 = Peripheral malfunction");
        return 0;
    }

    switch(lwm2m_carrier_error_code_add((int32_t)atoi(argv[1])))
    {
        case 0:
            shell_print(shell, "Error code added successfully");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported error code");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    }

    return 0;
}


static int cmd_device_error_code_remove(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, " 0 = No error");
        shell_print(shell, " 1 = Low charge");
        shell_print(shell, " 2 = External supply off");
        shell_print(shell, " 3 = GPS failure");
        shell_print(shell, " 4 = Low signal");
        shell_print(shell, " 5 = Out of memory");
        shell_print(shell, " 6 = SMS failure");
        shell_print(shell, " 7 = IP connectivity failure");
        shell_print(shell, " 8 = Peripheral malfunction");
        return 0;
    }

    switch(lwm2m_carrier_error_code_remove((int32_t)atoi(argv[1])))
    {
        case 0:
            shell_print(shell, "Error code removed successfully");
            break;
        case -ENOENT:
            shell_print(shell, "Error code not found");
            break;
        case -EINVAL:
            shell_print(shell, "Unsupported error code");
            break;
        default:
            shell_print(shell, "Error: %d", errno);
            break;
    }

    return 0;
}


static char *lwm2m_string_get(const lwm2m_string_t *string)
{
    static char string_buf[200];

    if (string->len >= 200)
    {
        return "<error>";
    }
    memcpy(string_buf, string->p_val, string->len);
    string_buf[string->len] = '\0';

    return string_buf;
}


static int cmd_device_print(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_device_t *device_obj_instance = lwm2m_device_get_instance(0);
    char buf[128];
    int offset = 0;

    for (int i = 0; i < device_obj_instance->avail_power_sources.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, " %d       ",
                           device_obj_instance->avail_power_sources.val.p_uint8[i]);
    }
    shell_print(shell, "Power sources    %s", buf);

    offset = 0;
    for (int i = 0; i < device_obj_instance->power_source_voltage.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%4d mV  ",
                           device_obj_instance->power_source_voltage.val.p_int32[i]);
    }
    shell_print(shell, "  Voltage         %s", buf);

    offset = 0;
    for (int i = 0; i < device_obj_instance->power_source_current.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%4d mA  ",
                           device_obj_instance->power_source_current.val.p_int32[i]);
    }
    shell_print(shell, "  Current         %s", buf);

    shell_print(shell, "Battery level     %d%%", device_obj_instance->battery_level);
    shell_print(shell, "Battery status    %d", device_obj_instance->battery_status);
    shell_print(shell, "Device type       %s", lwm2m_string_get(&device_obj_instance->device_type));
    shell_print(shell, "Hardware version  %s", lwm2m_string_get(&device_obj_instance->hardware_version));
    shell_print(shell, "Software version  %s", lwm2m_string_get(&device_obj_instance->software_version));
    shell_print(shell, "Total memory      %d kB", device_obj_instance->memory_total);
    shell_print(shell, "Memory free       %d kB", lwm2m_device_memory_free_read());

    offset = 0;
    for (int i = 0; i < device_obj_instance->error_code.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%d ",
                           device_obj_instance->error_code.val.p_int32[i]);
    }
    shell_print(shell, "Error codes       %s", buf);

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
    SHELL_CMD(logging, NULL, "Set logging value", cmd_debug_logging),
    SHELL_CMD(carrier_check, NULL, "Set carrier check", cmd_debug_carrier_check),
    SHELL_CMD(ipv6_enable, NULL, "Set IPv6 enabled", cmd_debug_ipv6_enabled),
    SHELL_CMD(fallback, NULL, "Set IP Fallback", cmd_debug_fallback_disabled),
    SHELL_CMD(con_interval, NULL, "Set CoAP CON timer", cmd_debug_con_interval),
    SHELL_CMD(net_reg_stat, NULL, "Set network registration status", cmd_debug_set_net_reg_stat),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_lwm2m,
    SHELL_CMD(status, NULL, "Application status", cmd_lwm2m_status),
    SHELL_CMD(register, NULL, "Register server", cmd_lwm2m_register),
    SHELL_CMD(update, NULL, "Update server", cmd_lwm2m_update),
    SHELL_CMD(deregister, NULL, "Deregister server", cmd_lwm2m_deregister),
    SHELL_CMD(disconnect, NULL, "Disconnect server", cmd_lwm2m_disconnect),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_device,
    SHELL_CMD(battery_level, NULL, "Set battery level", cmd_device_battery_level_set),
    SHELL_CMD(battery_status, NULL, "Set battery status", cmd_device_battery_status_set),
    SHELL_CMD(current, NULL, "Set current measurement on a power source", cmd_device_current_set),
    SHELL_CMD(device_type, NULL, "Set device type", cmd_device_type_set),
    SHELL_CMD(error_code_add, NULL, "Add individual error code", cmd_device_error_code_add),
    SHELL_CMD(error_code_remove, NULL, "Remove individual error code", cmd_device_error_code_remove),
    SHELL_CMD(hardware_version, NULL, "Set hardware version", cmd_device_hardware_version_set),
    SHELL_CMD(memory_free, NULL, "Set available amount of storage space", cmd_device_memory_free_write),
    SHELL_CMD(memory_total, NULL, "Set total amount of storage space", cmd_device_memory_total_set),
    SHELL_CMD(power_sources, NULL, "Set available device power sources", cmd_device_power_sources_set),
    SHELL_CMD(print, NULL, "Print all values set", cmd_device_print),
    SHELL_CMD(software_version, NULL, "Set software version", cmd_device_software_version_set),
    SHELL_CMD(voltage, NULL, "Set voltage measurement on a power source", cmd_device_voltage_set),
    SHELL_SUBCMD_SET_END
);


SHELL_CMD_REGISTER(at, NULL, "Send AT command", cmd_at_command);
SHELL_CMD_REGISTER(config, &sub_config, "Instance configuration", NULL);
SHELL_CMD_REGISTER(debug, &sub_debug, "Debug configuration", NULL);
SHELL_CMD_REGISTER(device, &sub_device, "Update or retrieve device information", NULL);
SHELL_CMD_REGISTER(lwm2m, &sub_lwm2m, "LwM2M operations", NULL);
SHELL_CMD_REGISTER(reboot, NULL, "Reboot", cmd_reboot);
SHELL_CMD_REGISTER(shutdown, NULL, "Shutdown", cmd_shutdown);


#endif // CONFIG_SHELL
