/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#if CONFIG_SHELL

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <shell/shell.h>
#include <fcntl.h>

#include <lwm2m_api.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_device.h>
#include <lwm2m_conn_mon.h>
#include <lwm2m_conn_ext.h>
#include <lwm2m_portfolio.h>
#include <lwm2m_apn_conn_prof.h>
#include <lwm2m_retry_delay.h>
#include <lwm2m_instance_storage.h>
#include <at_interface.h>
#include <operator_check.h>
#include <sms_receive.h>
#include <lwm2m_carrier_main.h>
#include <modem_logging.h>
#include <lwm2m_carrier.h>
#include <lwm2m_objects.h>
#include <lwm2m_observer.h>
#include <lwm2m_os.h>

static int cmd_at_command(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s \"AT command\"", argv[0]);
        return 0;
    }

    (void)modem_at_write(argv[1], true);

    return 0;
}

static int cmd_nslookup(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(shell, "%s [-4|-6] name [apn]", argv[0]);
        return 0;
    }

    struct nrf_addrinfo hints = {
        .ai_socktype = NRF_SOCK_DGRAM
    };
    struct nrf_addrinfo *p_hints = NULL;
    int argoff = 1;

    if (argv[argoff][0] == '-') {
        if (strcmp(argv[argoff], "-4") == 0) {
            hints.ai_family = NRF_AF_INET;
        } else if (strcmp(argv[argoff], "-6") == 0) {
            hints.ai_family = NRF_AF_INET6;
        } else {
            shell_print(shell, "invalid argument: %s", argv[argoff]);
            return 0;
        }
        p_hints = &hints;
        argoff++;
    }

    char *hostname = argv[argoff++];

    struct nrf_addrinfo apn_hints;
    if (argc > argoff) {
        apn_hints.ai_family    = NRF_AF_LTE;
        apn_hints.ai_socktype  = NRF_SOCK_MGMT;
        apn_hints.ai_protocol  = NRF_PROTO_PDN;
        apn_hints.ai_canonname = argv[argoff];

        hints.ai_next = &apn_hints;
        if (!p_hints) {
            // Need to hint family when specifying APN
            hints.ai_family = NRF_AF_INET;
            p_hints = &hints;
        }
    }

    struct nrf_addrinfo *result;
    int ret_val = nrf_getaddrinfo(hostname, NULL, p_hints, &result);

    if (ret_val != 0) {
        shell_print(shell, "error: %d", ret_val);
        return 0;
    }

    char ip_buffer[64];
    void *p_addr;
    while (result) {
        switch (result->ai_family) {
        case NRF_AF_INET:
            p_addr = &((struct nrf_sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
            nrf_inet_ntop(result->ai_family, p_addr, ip_buffer, sizeof(ip_buffer));
            break;
        case NRF_AF_INET6:
            p_addr = &((struct nrf_sockaddr_in6 *)result->ai_addr)->sin6_addr.s6_addr;
            nrf_inet_ntop(result->ai_family, p_addr, ip_buffer, sizeof(ip_buffer));
            break;
        default:
            snprintf(ip_buffer, sizeof(ip_buffer), "Unknown family: %d", result->ai_family);
            break;
        }

        // TODO: Print result->ai_canonname when NCSDK-4823 is fixed
        shell_print(shell, "Name:    %s", hostname);
        shell_print(shell, "Address:  %s", ip_buffer);

        result = result->ai_next;
    }

    nrf_freeaddrinfo(result);

    return 0;
}


static int cmd_security_print(const struct shell *shell, size_t argc, char **argv)
{
    uint8_t uri_len = 0;
    // Buffer for the URI with null terminator
    char terminated_uri[128];

    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++) {
        if (lwm2m_security_short_server_id_get(i) != 0) {
            char * server_uri = lwm2m_security_server_uri_get(i, &uri_len);
            if (uri_len > 127) {
                uri_len = 127;
            }
            memcpy(terminated_uri, server_uri, uri_len);
            terminated_uri[uri_len] = '\0';

            shell_print(shell, "Security Instance /0/%d", i);
            shell_print(shell, "  Short Server ID  %d", lwm2m_security_short_server_id_get(i));
            shell_print(shell, "  Server URI       %s", terminated_uri);
            shell_print(shell, "  Bootstrap Server %s", lwm2m_security_is_bootstrap_server_get(i) ? "Yes" : "No");
            shell_print(shell, "  Client Holdoff   %ld", lwm2m_server_client_hold_off_timer_get(i));

            if (operator_is_vzw(true) && lwm2m_security_is_bootstrap_server_get(i)) {
                shell_print(shell, "  Holdoff          %ld", lwm2m_security_hold_off_timer_get(i));
                shell_print(shell, "  Is Bootstrapped  %s", lwm2m_security_bootstrapped_get(i) ? "Yes" : "No");
            }
        }
    }

    return 0;
}


static int cmd_security_uri(const struct shell *shell, size_t argc, char **argv)
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
    lwm2m_storage_security_store();

    shell_print(shell, "Set URI %d: %s", instance_id, uri);

    return 0;
}


static int cmd_server_print(const struct shell *shell, size_t argc, char **argv)
{
    uint8_t binding_len = 0;
    char binding[4];

    uint16_t bootstrap_ssid = lwm2m_security_short_server_id_get(0);

    for (int i = 0; i < (1+LWM2M_MAX_SERVERS); i++) {
        if (lwm2m_server_short_server_id_get(i) != 0) {
            lwm2m_instance_t *p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(i);
            char * p_binding = lwm2m_server_binding_get(i, &binding_len);
            if (binding_len > sizeof(binding) - 1) {
                binding_len = sizeof(binding) - 1;
            }
            memcpy(binding, p_binding, binding_len);
            binding[binding_len] = '\0';

            shell_print(shell, "Server Instance /1/%d", i);
            shell_print(shell, "  Short Server ID  %d", lwm2m_server_short_server_id_get(i));
            shell_print(shell, "  Lifetime         %ld", lwm2m_server_lifetime_get(i));
            shell_print(shell, "  Min Period       %ld", lwm2m_server_min_period_get(i));
            shell_print(shell, "  Max Period       %ld", lwm2m_server_max_period_get(i));
            shell_print(shell, "  Disable Timeout  %ld", lwm2m_server_disable_timeout_get(i));
            shell_print(shell, "  Notif Storing    %s", lwm2m_server_notif_storing_get(i) ? "Yes" : "No");
            shell_print(shell, "  Binding          %s", binding);

            if (operator_is_vzw(true) && lwm2m_server_short_server_id_get(i) != bootstrap_ssid) {
                shell_print(shell, "  Is Registered    %s", lwm2m_server_registered_get(i) ? "Yes" : "No");
                shell_print(shell, "  Client Holdoff   %ld", lwm2m_server_client_hold_off_timer_get(i));
            }

            shell_print(shell, "  Owner            %d", p_instance->acl.owner);
        }
    }

    return 0;
}


static int cmd_server_lifetime(const struct shell *shell, size_t argc, char **argv)
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
        lwm2m_storage_server_store();

        shell_print(shell, "Set lifetime %d: %d", instance_id, lifetime);
    }

    return 0;
}


static int cmd_debug_print(const struct shell *shell, size_t argc, char **argv)
{
    char client_id[LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN+1];
    uint16_t client_id_len;
    char *p_client_id = lwm2m_client_id_get(&client_id_len);
    client_id_len = MIN(client_id_len, LWM2M_CLIENT_ID_TYPE_IMEI_MSISDN);
    memcpy(client_id, p_client_id, client_id_len);
    client_id[client_id_len] = '\0';

    uint32_t iccid_len;
    char * p_iccid = lwm2m_device_get_sim_iccid(&iccid_len);
    char iccid[21];
    if (p_iccid) {
        memcpy(iccid, p_iccid, 20);
        iccid[iccid_len] = 0;
    } else {
        iccid[0] = 0;
    }

    char last_used_msisdn[16];
    int32_t len = lwm2m_last_used_msisdn_get(last_used_msisdn, sizeof(last_used_msisdn));
    len = MIN(len, 15);
    last_used_msisdn[len > 0 ? len : 0] = 0;

    shell_print(shell, "Debug configuration");
    shell_print(shell, "  Client ID      %s", client_id);
    shell_print(shell, "  IMEI           %s", lwm2m_imei_get());
    shell_print(shell, "  SIM MSISDN     %s", lwm2m_msisdn_get());
    shell_print(shell, "  SIM ICCID      %s", iccid);
    shell_print(shell, "  Stored MSISDN  %s", last_used_msisdn);
    shell_print(shell, "  Logging        %s", modem_logging_get());
    shell_print(shell, "  Real carrier   %s", operator_id_string(OPERATOR_ID_CURRENT));
    shell_print(shell, "  Carrier check  %s", lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK) ? "No" : "Yes");
    if (lwm2m_debug_is_set(LWM2M_DEBUG_DISABLE_CARRIER_CHECK)) {
        uint32_t operator_id = lwm2m_debug_operator_id_get();
        shell_print(shell, "   Debug carrier %u (%s)", operator_id, operator_id_string(operator_id));
    }
    shell_print(shell, "  Roam as Home   %s", lwm2m_debug_is_set(LWM2M_DEBUG_ROAM_AS_HOME) ? "Yes" : "No");
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

static int cmd_debug_msisdn(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s MSISDN", argv[0]);
        return 0;
    }

    char *p_msisdn = argv[1];
    size_t msisdn_len = strlen(p_msisdn);

    if (msisdn_len > 15) {
        shell_print(shell, "length of MSISDN must be less than 15");
        return 0;
    }

    lwm2m_last_used_msisdn_set(p_msisdn, msisdn_len);
    lwm2m_conn_ext_msisdn_set(p_msisdn, msisdn_len);

    for (uint32_t i = 1; i < 1 + LWM2M_MAX_SERVERS; i++) {
        lwm2m_request_server_update(i, false);
    }

    if (msisdn_len) {
        shell_print(shell, "Set MSISDN: %s", p_msisdn);
    } else {
        shell_print(shell, "Removed MSISDN");
    }

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

static int cmd_debug_roam_as_home(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_print(shell, "%s <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int roam_as_home = atoi(argv[1]);

    if (roam_as_home != 0 && roam_as_home != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (roam_as_home) {
        lwm2m_debug_set(LWM2M_DEBUG_ROAM_AS_HOME);
    } else {
        lwm2m_debug_clear(LWM2M_DEBUG_ROAM_AS_HOME);
    }

    shell_print(shell, "Set roam as home: %d", roam_as_home);

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

static int cmd_debug_operator_id(const struct shell *shell, size_t argc, char **argv)
{
    uint32_t operator_max = operator_id_max();

    if (argc != 2) {
        shell_print(shell, "%s <id>", argv[0]);
        for (int i = 0; i <= operator_max; i++) {
            shell_print(shell, " %u = %s", i, operator_id_string(i));
        }
        return 0;
    }

    int operator_id = atoi(argv[1]);

    if (operator_id < 0 || operator_id > operator_max) {
        shell_print(shell, "invalid value, must be between 0 and %u", operator_max);
        return 0;
    }

    lwm2m_debug_operator_id_set(operator_id);

    shell_print(shell, "Set carrier: %u (%s)", operator_id, operator_id_string(operator_id));

    return 0;
}

static int cmd_flash_list(const struct shell *shell, size_t argc, char **argv)
{
    int read;
    char buf[1];

    shell_print(shell, "Record range 0x%4X - 0x%4X", LWM2M_OS_STORAGE_BASE, LWM2M_OS_STORAGE_END);

    for (int i = LWM2M_OS_STORAGE_BASE; i < LWM2M_OS_STORAGE_END; i++) {
        read = lwm2m_os_storage_read(i, buf, 1);
        if (read > 0) {
            shell_print(shell, "  Record %d (%d bytes)", i - LWM2M_OS_STORAGE_BASE, read);
        }
    }

    return 0;
}

static void dump_as_hex(const struct shell *shell, char *data, int len)
{
    static const char hexchars[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int pos = 0;

    while (pos < len) {
        char hexbuf[256];
        char *cur = hexbuf;
        int i;

        // Dump offset
        cur += snprintf(cur, 20, "  %04X  ", pos);

        // Dump bytes as hex
        for (i = 0; i < 16 && pos + i < len; i++) {
            *cur++ = hexchars[(data[pos + i] & 0xf0) >> 4];
            *cur++ = hexchars[data[pos + i] & 0x0f];
            *cur++ = ' ';
            if (i == 7) {
                *cur++ = ' ';
            }
        }

        while (cur < hexbuf + 58) {
            *cur++ = ' '; // Fill it up with space to ascii column
        }

        // Dump bytes as text
        for (i = 0; i < 16 && pos + i < len; i++) {
            if (isprint((int)data[pos + i])) {
                *cur++ = data[pos + i];
            } else {
                *cur++ = '.';
            }
            if (i == 7) {
                *cur++ = ' ';
            }
        }

        pos += i;
        *cur = 0;

        shell_print(shell, "%s", hexbuf);
    }
}

static void dump_record_content(const struct shell *shell, uint16_t id, char *data, int len)
{
    switch (id) {
        // TODO: Dump according to content of record: TLV, struct, string, etc.
        default:
            dump_as_hex(shell, data, len);
            break;
    }
}

static bool dump_record(const struct shell *shell, uint16_t id)
{
    char buf[1];
    int read = lwm2m_os_storage_read(id, buf, 1);

    if (read <= 0) {
        return false;
    }

    shell_print(shell, "Record %d - %d bytes", id - LWM2M_OS_STORAGE_BASE, read);

    char * data = lwm2m_os_malloc(read);
    lwm2m_os_storage_read(id, data, read);
    dump_record_content(shell, id, data, read);
    lwm2m_os_free(data);

    return true;
}

static int cmd_flash_print(const struct shell *shell, size_t argc, char **argv)
{
    if (argc > 2) {
        shell_print(shell, "%s [record]", argv[0]);
        return 0;
    }

    if (argc == 2) {
        uint16_t id = strtol(argv[1], NULL, 10);
        if (!dump_record(shell, LWM2M_OS_STORAGE_BASE + id)) {
            shell_print(shell, "Record %d does not exist", id);
        }
    } else {
        for (int i = LWM2M_OS_STORAGE_BASE; i < LWM2M_OS_STORAGE_END; i++) {
            (void)dump_record(shell, i);
        }
    }

    return 0;
}

static int cmd_lwm2m_bootstrap(const struct shell *shell, size_t argc, char **argv)
{
    if ((lwm2m_state_get() == LWM2M_STATE_IDLE) ||
        (lwm2m_state_get() == LWM2M_STATE_DISCONNECTED))
    {
        lwm2m_request_bootstrap();
    } else {
        shell_print(shell, "Wrong state for bootstrap");
    }

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
        lwm2m_request_server_update(instance_id, true);
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
    if (argc == 2) {
        uint16_t instance_id = atoi(argv[1]);

        if (instance_id < 1 || instance_id >= (1+LWM2M_MAX_SERVERS))
        {
            shell_print(shell, "instance must be between 1 and %d", LWM2M_MAX_SERVERS);
            return 0;
        }

        lwm2m_request_server_disconnect(instance_id);
        return 0;
    }

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
    ip_version[3] = (lwm2m_family_type_get(lwm2m_security_instance()) == NRF_AF_INET6) ? '6' : '4';
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
            retry_delay = lwm2m_retry_delay_connect_get(0, NULL);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / K_SECONDS(1);
                shell_print(shell, "Bootstrap connect delay: %d minutes (%d seconds left) [%s]",
                            retry_delay / K_MINUTES(1), delay, ip_version);
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
            retry_delay = lwm2m_retry_delay_connect_get(0, NULL);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / K_SECONDS(1);
                shell_print(shell, "Bootstrap delay: %d minutes (%d seconds left) [%s]",
                            retry_delay / K_MINUTES(1), delay, ip_version);
            } else {
                shell_print(shell, "Bootstrap wait [%s]", ip_version);
            }
            break;
        case LWM2M_STATE_BOOTSTRAPPING:
            shell_print(shell, "Bootstrapping [%s]", ip_version);
            break;
        case LWM2M_STATE_CLIENT_HOLD_OFF:
            shell_print(shell, "Client hold off (server %d)", lwm2m_security_instance());
            break;
        case LWM2M_STATE_SERVER_CONNECT:
            shell_print(shell, "Server %d connecting [%s]", lwm2m_security_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_CONNECT_WAIT:
            shell_print(shell, "Server %d connect wait [%s]", lwm2m_security_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_CONNECT_RETRY_WAIT:
            retry_delay = lwm2m_retry_delay_connect_get(lwm2m_security_instance(), NULL);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / K_SECONDS(1);
                shell_print(shell, "Server %d connect delay: %d minutes (%d seconds left) [%s]",
                            lwm2m_security_instance(), retry_delay / K_MINUTES(1), delay, ip_version);
            } else {
                shell_print(shell, "Server %d connect timed wait [%s]", lwm2m_security_instance(), ip_version);
            }
            break;
        case LWM2M_STATE_SERVER_CONNECTED:
            shell_print(shell, "Server %d connected [%s]", lwm2m_security_instance(), ip_version);
            break;
        case LWM2M_STATE_SERVER_REGISTER_WAIT:
            retry_delay = lwm2m_retry_delay_connect_get(lwm2m_security_instance(), NULL);
            if (retry_delay != -1) {
                int32_t delay = lwm2m_state_update_delay() / K_SECONDS(1);
                shell_print(shell, "Server %d register delay: %d minutes (%d seconds left) [%s]",
                            lwm2m_security_instance(), retry_delay / K_MINUTES(1), delay, ip_version);
            } else {
                shell_print(shell, "Server %d register wait [%s]", lwm2m_security_instance(), ip_version);
            }
            break;
        case LWM2M_STATE_SERVER_DEREGISTER:
            shell_print(shell, "Server %d deregister", lwm2m_security_instance());
            break;
        case LWM2M_STATE_SERVER_DEREGISTERING:
            shell_print(shell, "Server %d deregistering", lwm2m_security_instance());
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


static int cmd_reboot(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_system_reset(true);

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

    shell_print(shell, "Device Instance /3/0");

    for (int i = 0; i < device_obj_instance->avail_power_sources.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, " %d       ",
                           device_obj_instance->avail_power_sources.val.p_uint8[i]);
    }
    shell_print(shell, "  Power sources    %s", buf);

    offset = 0;
    for (int i = 0; i < device_obj_instance->power_source_voltage.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%4d mV  ",
                           device_obj_instance->power_source_voltage.val.p_int32[i]);
    }
    shell_print(shell, "    Voltage         %s", buf);

    offset = 0;
    for (int i = 0; i < device_obj_instance->power_source_current.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%4d mA  ",
                           device_obj_instance->power_source_current.val.p_int32[i]);
    }
    shell_print(shell, "    Current         %s", buf);

    shell_print(shell, "  Battery level     %d%%", device_obj_instance->battery_level);
    shell_print(shell, "  Battery status    %d", device_obj_instance->battery_status);
    shell_print(shell, "  Manufacturer      %s", lwm2m_string_get(&device_obj_instance->manufacturer));
    shell_print(shell, "  Model number      %s", lwm2m_string_get(&device_obj_instance->model_number));
    shell_print(shell, "  Serial number     %s", lwm2m_string_get(&device_obj_instance->serial_number));
    shell_print(shell, "  Firmware version  %s", lwm2m_string_get(&device_obj_instance->firmware_version));
    shell_print(shell, "  Device type       %s", lwm2m_string_get(&device_obj_instance->device_type));
    shell_print(shell, "  Hardware version  %s", lwm2m_string_get(&device_obj_instance->hardware_version));
    shell_print(shell, "  Software version  %s", lwm2m_string_get(&device_obj_instance->software_version));
    shell_print(shell, "  Total memory      %d kB", device_obj_instance->memory_total);
    shell_print(shell, "  Memory free       %d kB", lwm2m_device_memory_free_read());

    offset = 0;
    for (int i = 0; i < device_obj_instance->error_code.len; i++)
    {
        offset += snprintf(&buf[offset], sizeof(buf) - offset, "%d ",
                           device_obj_instance->error_code.val.p_int32[i]);
    }
    shell_print(shell, "  Error codes       %s", buf);

    return 0;
}


static int cmd_device_bootstrap_clear(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_bootstrap_clear();
    shell_print(shell, "Cleared bootstrapped");

    return 0;
}


static int cmd_device_factory_reset(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_factory_reset();
    lwm2m_request_reset();

    return 0;
}


static int cmd_apn_write_class(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_print(shell, "%s <class> <APN>", argv[0]);
        return 0;
    }

    char *p_end;
    int class = strtol(argv[1], &p_end, 10);
    if ((class < 1) || (class > 10))
    {
        shell_print(shell, "Invalid APN Class: %u", class);
        return 0;
    }

    char * p_apn = argv[2];
    shell_print(shell, "Write APN Class %d: %s", class, p_apn);

    lwm2m_conn_mon_class_apn_set(class, p_apn, strlen(p_apn));

    return 0;
}

static int cmd_apn_read_class(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "%s <class>", argv[0]);
        return 0;
    }

    char buffer[64];

    char *p_end;
    int class = strtol(argv[1], &p_end, 10);
    if ((class < 1) || (class > 10))
    {
        shell_print(shell, "Invalid APN Class: %u", class);
        return 0;
    }

    uint8_t len = 0;
    char * p_apn = lwm2m_conn_mon_class_apn_get(class, &len);

    memcpy(buffer, p_apn, len);
    buffer[len] = '\0';

    shell_print(shell, "Read APN Class %d: %s", class, buffer);

    return 0;
}


static int cmd_apn_activate(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2 && argc != 3)
    {
        shell_print(shell, "%s <instance> <reject_cause[=0]>", argv[0]);
        return 0;
    }

    char *p_end;
    int instance_id = strtol(argv[1], &p_end, 10);
    uint8_t reject_cause = 0;

    if (argc == 3)
    {
        reject_cause = (uint8_t) strtol(argv[2], &p_end, 10);
    }

    if (!lwm2m_apn_conn_prof_activate(instance_id, reject_cause)) {
        shell_print(shell, "Illegal instance: %d", instance_id);
    }
    return 0;
}


static int cmd_apn_deactivate(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "%s <instance>", argv[0]);
        return 0;
    }

    int instance_id = atoi(argv[1]);

    if (!lwm2m_apn_conn_prof_deactivate(instance_id)) {
        shell_print(shell, "Illegal instance: %d", instance_id);
    }

    return 0;
}

static int cmd_apn_enable_status(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_print(shell, "%s <instance> <value>", argv[0]);
        shell_print(shell, " 0 = disable");
        shell_print(shell, " 1 = enable");
        return 0;
    }

    int instance_id = strtol(argv[1], NULL, 10);
    int enable_status = strtol(argv[2], NULL, 10);

    if (enable_status != 0 && enable_status != 1) {
        shell_print(shell, "invalid value, must be 0 or 1");
        return 0;
    }

    if (!lwm2m_apn_conn_prof_enabled_set(instance_id, enable_status)) {
        shell_print(shell, "Illegal instance: %d", instance_id);
    }

    return 0;
}


#define TIME_STR_SIZE sizeof("1970-01-01T00:00:00Z")

static void utc_time(int32_t timestamp, char *time_str)
{
    time_t time = (time_t)timestamp;
    struct tm *tm = gmtime(&time);

    strftime(time_str, TIME_STR_SIZE, "%FT%TZ", tm);
}

static int cmd_apn_print(const struct shell *shell, size_t argc, char **argv)
{
    char start_time_str[TIME_STR_SIZE];
    char end_time_str[TIME_STR_SIZE];

    for (int i = 0; i < LWM2M_MAX_APN_COUNT; i++) {
        lwm2m_apn_conn_prof_t * p_apn_conn = lwm2m_apn_conn_prof_get_instance(i);

        if (!p_apn_conn || !p_apn_conn->apn.p_val)
        {
            continue;
        }

        shell_print(shell, "APN Connection Profile Instance /11/%d", i);
        shell_print(shell, "  Profile Name   %s", lwm2m_string_get(&p_apn_conn->profile_name));
        shell_print(shell, "  APN            %s", lwm2m_string_get(&p_apn_conn->apn));
        shell_print(shell, "  Enable status  %s", p_apn_conn->enable_status ? "activated" : "deactivated");
        shell_print(shell, "  Connection     Start time            Result  Cause  End time");
        for (int j = 0; j < p_apn_conn->conn_est_time.len; j++) {
            utc_time(lwm2m_list_integer_get(&p_apn_conn->conn_est_time, j), start_time_str);
            utc_time(lwm2m_list_integer_get(&p_apn_conn->conn_end_time, j), end_time_str);
            shell_print(shell, "    %1d            %s  %6d  %5d  %s", j,
                        start_time_str,
                        lwm2m_list_integer_get(&p_apn_conn->conn_est_result, j),
                        lwm2m_list_integer_get(&p_apn_conn->conn_est_reject_cause, j),
                        end_time_str);
        }
    }

    return 0;
}


// Compare if obs1 is greater than obs2
static bool observable_greater_than(lwm2m_observable_metadata_t *obs1, lwm2m_observable_metadata_t *obs2)
{
    if (obs1 == NULL) {
        return true;
    }

    if (obs2 == NULL) {
        return false;
    }

    for (int i = 0; i < obs1->path_len; i++)
    {
        if ((i >= obs2->path_len) ||
            (obs1->path[i] > obs2->path[i]))
        {
            return true;
        }

        if (obs1->path[i] < obs2->path[i])
        {
            return false;
        }
    }

    return false;
}


static int cmd_attribute_print(const struct shell *shell, size_t argc, char **argv)
{
    char buf[255];
    uint16_t len;
    int offset;
    const lwm2m_observable_metadata_t * const * observables = lwm2m_observables_get(&len);
    lwm2m_observable_metadata_t *observables_srt[len];
    lwm2m_observable_metadata_t *obs;
    const char *lwm2m_notif_attribute_name[] = { "pmin", "pmax", "gt",
                                                "lt", "st" };

    if (!observables)
    {
        return 0;
    }

    memcpy(observables_srt, observables, sizeof(observables_srt));

    for (int i = 1; i < len; i++)
    {
        obs = observables_srt[i];

        int j = i - 1;
        while (j >= 0 && observable_greater_than(observables_srt[j], obs))
        {
            observables_srt[j + 1] = observables_srt[j];
            j--;
        }

        observables_srt[j + 1] = obs;
    }

    for (int i = 0; i < len; i++)
    {
        if (!observables_srt[i])
        {
            continue;
        }

        memset(buf, 0, sizeof(buf));
        offset = 0;

        offset += snprintf(&buf[offset], sizeof(buf) - offset, "<");

        for (int j = 0; j < observables_srt[i]->path_len; j++)
        {
            offset += snprintf(&buf[offset], sizeof(buf) - offset, "/%d", observables_srt[i]->path[j]);
        }

        offset += snprintf(&buf[offset], sizeof(buf) - offset, ">; ssid=%d;", observables_srt[i]->ssid);

        for (int k = 0; k < LWM2M_MAX_NOTIF_ATTRIBUTE_TYPE; k++)
        {
            if (observables_srt[i]->attributes[k].assignment_level != LWM2M_ATTR_UNINIT_ASSIGNMENT_LEVEL)
            {
                if (observables_srt[i]->path_len != observables_srt[i]->attributes[k].assignment_level)
                {
                    offset += snprintf(&buf[offset], sizeof(buf) - offset, " [%d", observables_srt[i]->attributes[k].assignment_level);
                }

                offset += snprintf(&buf[offset], sizeof(buf) - offset, " %s=%d;", lwm2m_notif_attribute_name[k], observables_srt[i]->attributes[k].value.i);

                if (observables_srt[i]->path_len != observables_srt[i]->attributes[k].assignment_level)
                {
                    offset += snprintf(&buf[offset], sizeof(buf) - offset, "]");
                }
            }
        }

        buf[offset] = '\0';

        shell_print(shell, "%s", buf);
    }

    return 0;
}


static int cmd_portfolio_print(const struct shell *shell, size_t argc, char **argv)
{
    lwm2m_list_t *p_list;

    for (int i = 0; i < LWM2M_PORTFOLIO_MAX_INSTANCES; i++)
    {
        lwm2m_portfolio_t *p_instance = lwm2m_portfolio_get_instance(i);
        if (!p_instance)
        {
            continue;
        }

        p_list = &p_instance->identity;

        shell_print(shell, "Portfolio Instance /16/%d", i);
        shell_print(shell, "  Host Device ID                %s", lwm2m_string_get(lwm2m_list_string_get(p_list, 0)));
        shell_print(shell, "  Host Device Manufacturer      %s", lwm2m_string_get(lwm2m_list_string_get(p_list, 1)));
        shell_print(shell, "  Host Device Model             %s", lwm2m_string_get(lwm2m_list_string_get(p_list, 2)));
        shell_print(shell, "  Host Device Software Version  %s", lwm2m_string_get(lwm2m_list_string_get(p_list, 3)));
    }

    return 0;
}


static int cmd_portfolio_read(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_print(shell, "%s <object instance> <resource instance>", argv[0]);
        return 0;
    }

    int ret;
    uint16_t instance_id;
    uint16_t identity_type;
    char buffer[200];
    uint16_t len = sizeof(buffer);

    instance_id = (uint16_t)atoi(argv[1]);
    identity_type = (uint16_t)atoi(argv[2]);

    ret = lwm2m_carrier_identity_read(instance_id, identity_type, buffer, &len);

    switch (ret)
    {
    case 0:
        shell_print(shell, "%s", buffer);
        break;
    case -ENOMEM:
        shell_print(shell, "Insufficient memory");
        break;
    case -ENOENT:
        shell_print(shell, "Object instance %d does not exist", instance_id);
        break;
    case -EINVAL:
        shell_print(shell, "Invalid Identity type %d", identity_type);
        break;
    default:
        shell_print(shell, "Unknown error %d", ret);
        break;
    }

    return 0;
}


static int cmd_portfolio_write(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 4)
    {
        shell_print(shell, "%s <object instance> <resource instance> <value>", argv[0]);
        return 0;
    }

    int ret;
    uint16_t instance_id;
    uint16_t identity_type;
    const char *val;

    instance_id = (int)atoi(argv[1]);
    identity_type = (int)atoi(argv[2]);
    val = argv[3];

    ret = lwm2m_carrier_identity_write(instance_id, identity_type, val);

    switch (ret)
    {
    case 0:
        shell_print(shell, "Wrote /16/%d/0/%d", instance_id, identity_type);
        break;
    case -ENOENT:
        shell_print(shell, "Object instance %d does not exist", instance_id);
        break;
    case -ENOMEM:
        shell_print(shell, "Insufficient memory");
        break;
    case -EINVAL:
        shell_print(shell, "String is NULL or empty, or invalid Identity type %d", identity_type);
        break;
    case -E2BIG:
        shell_print(shell, "String is too long");
        break;
    case -EPERM:
        shell_print(shell, "Cannot write to instance %d", instance_id);
        break;
    default:
        shell_print(shell, "Unknown error %d", ret);
        break;
    }

    return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_security,
    SHELL_CMD(print, NULL, "Print security objects", cmd_security_print),
    SHELL_CMD(uri, NULL, "Set URI", cmd_security_uri),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_server,
    SHELL_CMD(lifetime, NULL, "Set lifetime", cmd_server_lifetime),
    SHELL_CMD(print, NULL, "Print server objects", cmd_server_print),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_apn,
    SHELL_CMD(activate, NULL, "Activate APN", cmd_apn_activate),
    SHELL_CMD(deactivate, NULL, "Deactivate APN", cmd_apn_deactivate),
    SHELL_CMD(enable_status, NULL, "Set enable status", cmd_apn_enable_status),
    SHELL_CMD(print, NULL, "Print apn connection profile objects", cmd_apn_print),
    SHELL_CMD(read_class, NULL, "Read APN class", cmd_apn_read_class),
    SHELL_CMD(write_class, NULL, "Write APN class", cmd_apn_write_class),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_attribute,
    SHELL_CMD(print, NULL, "Print notification attributes", cmd_attribute_print),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_debug,
    SHELL_CMD(carrier, NULL, "Set debug carrier", cmd_debug_operator_id),
    SHELL_CMD(carrier_check, NULL, "Set carrier check", cmd_debug_carrier_check),
    SHELL_CMD(con_interval, NULL, "Set CoAP CON timer", cmd_debug_con_interval),
    SHELL_CMD(fallback, NULL, "Set IP Fallback", cmd_debug_fallback_disabled),
    SHELL_CMD(ipv6_enable, NULL, "Set IPv6 enabled", cmd_debug_ipv6_enabled),
    SHELL_CMD(logging, NULL, "Set logging value", cmd_debug_logging),
    SHELL_CMD(msisdn, NULL, "Set MSISDN", cmd_debug_msisdn),
    SHELL_CMD(net_reg_stat, NULL, "Set network registration status", cmd_debug_set_net_reg_stat),
    SHELL_CMD(print, NULL, "Print configuration", cmd_debug_print),
    SHELL_CMD(reset, NULL, "Reset configuration", cmd_debug_reset),
    SHELL_CMD(roam_as_home, NULL, "Set Roam as Home", cmd_debug_roam_as_home),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_flash,
    SHELL_CMD(list, NULL, "List records", cmd_flash_list),
    SHELL_CMD(print, NULL, "Print record content", cmd_flash_print),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lwm2m,
    SHELL_CMD(bootstrap, NULL, "Bootstrap", cmd_lwm2m_bootstrap),
    SHELL_CMD(deregister, NULL, "Deregister server", cmd_lwm2m_deregister),
    SHELL_CMD(disconnect, NULL, "Disconnect server", cmd_lwm2m_disconnect),
    SHELL_CMD(register, NULL, "Register server", cmd_lwm2m_register),
    SHELL_CMD(status, NULL, "Connection status", cmd_lwm2m_status),
    SHELL_CMD(update, NULL, "Update server", cmd_lwm2m_update),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_device,
    SHELL_CMD(battery_level, NULL, "Set battery level", cmd_device_battery_level_set),
    SHELL_CMD(battery_status, NULL, "Set battery status", cmd_device_battery_status_set),
    SHELL_CMD(clear, NULL, "Clear bootstrapped values", cmd_device_bootstrap_clear),
    SHELL_CMD(current, NULL, "Set current measurement on a power source", cmd_device_current_set),
    SHELL_CMD(device_type, NULL, "Set device type", cmd_device_type_set),
    SHELL_CMD(error_code_add, NULL, "Add individual error code", cmd_device_error_code_add),
    SHELL_CMD(error_code_remove, NULL, "Remove individual error code", cmd_device_error_code_remove),
    SHELL_CMD(factory_reset, NULL, "Factory reset", cmd_device_factory_reset),
    SHELL_CMD(hardware_version, NULL, "Set hardware version", cmd_device_hardware_version_set),
    SHELL_CMD(memory_free, NULL, "Set available amount of storage space", cmd_device_memory_free_write),
    SHELL_CMD(memory_total, NULL, "Set total amount of storage space", cmd_device_memory_total_set),
    SHELL_CMD(power_sources, NULL, "Set available device power sources", cmd_device_power_sources_set),
    SHELL_CMD(print, NULL, "Print all values set", cmd_device_print),
    SHELL_CMD(software_version, NULL, "Set software version", cmd_device_software_version_set),
    SHELL_CMD(voltage, NULL, "Set voltage measurement on a power source", cmd_device_voltage_set),
    SHELL_SUBCMD_SET_END
);


SHELL_STATIC_SUBCMD_SET_CREATE(sub_portfolio,
    SHELL_CMD(print, NULL, "Print portfolio object instances", cmd_portfolio_print),
    SHELL_CMD(read, NULL, "Read the Identity resource of a Portfolio object instance", cmd_portfolio_read),
    SHELL_CMD(write, NULL, "Write into an instance of the Identity resource", cmd_portfolio_write),
    SHELL_SUBCMD_SET_END
);


SHELL_CMD_REGISTER(apn, &sub_apn, "APN Table", NULL);
SHELL_CMD_REGISTER(at, NULL, "Send AT command", cmd_at_command);
SHELL_CMD_REGISTER(attribute, &sub_attribute, "Notification attributes operations", NULL);
SHELL_CMD_REGISTER(debug, &sub_debug, "Debug configuration", NULL);
SHELL_CMD_REGISTER(device, &sub_device, "Update or retrieve device information", NULL);
SHELL_CMD_REGISTER(flash, &sub_flash, "Flash operations", NULL);
SHELL_CMD_REGISTER(lwm2m, &sub_lwm2m, "LwM2M operations", NULL);
SHELL_CMD_REGISTER(nslookup, NULL, "Query Internet name servers", cmd_nslookup);
SHELL_CMD_REGISTER(portfolio, &sub_portfolio, "Portfolio object operations", NULL);
SHELL_CMD_REGISTER(reboot, NULL, "Reboot", cmd_reboot);
SHELL_CMD_REGISTER(security, &sub_security, "Security information", NULL);
SHELL_CMD_REGISTER(server, &sub_server, "Server information", NULL);
SHELL_CMD_REGISTER(shutdown, NULL, "Shutdown", cmd_shutdown);


#endif // CONFIG_SHELL
