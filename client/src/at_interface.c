/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <net/socket.h>
#include <pdn_management.h>

#define APP_MAX_AT_READ_LENGTH          256
#define APP_MAX_AT_WRITE_LENGTH         256


int at_apn_setup_wait_for_ipv6(char * apn)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int apn_handle = -1;

    int at_socket_fd;
    int at_socket_cgev_fd;

    if (apn == NULL) {
        return -1;
    }

    at_socket_cgev_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_cgev_fd < 0) {
        printk("socket() failed\n");
    }

    // Subscribe to CGEV
    int sent_cgev_length = send(at_socket_cgev_fd, "AT+CGEREP=1\r\n", sizeof("AT+CGEREP=1\r\n"), 0);
    if (sent_cgev_length == -1)
    {
        // Should still be -1.
        printk("IPv6 APN failed sending CGEREP=1\n");
        return -1;
    }

    int received_cgev_length = 0;
    do {
        memset(read_buffer, 0, sizeof(read_buffer));
        received_cgev_length = recv(at_socket_cgev_fd, read_buffer, sizeof(read_buffer), 0);
    } while (received_cgev_length <= 0);

    // Check if subscription went OK.
    if ((received_cgev_length <= 0) || strstr(read_buffer, "OK\r\n") == NULL)
    {
        printk("IPv6 APN CGERRP response not ok: %s\n", read_buffer);
        return -1;
    }

    // Set up APN which implicitly creates a CID.
    apn_handle = pdn_init_and_connect(apn);

    bool cid_found = false;
    int  cid_number = 0;
    if (apn_handle > -1)
    {
        at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
        if (at_socket_fd < 0) {
            printk("socket() failed\n");
        }

        // Loop through possible CID to and lookup the APN.
        for (; cid_number < 12; cid_number++) {
            snprintf(write_buffer, sizeof(write_buffer), "AT+CGCONTRDP=%u\r\n",  cid_number);
            int sent_length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);
            if (sent_length == -1)
            {
                break;
            }
            memset(read_buffer, 0, sizeof(read_buffer));
            int revieved_length = recv(at_socket_fd, read_buffer, sizeof(read_buffer), 0);

            if ((revieved_length > 0) && (strstr(read_buffer, apn) != NULL))
            {
                cid_found = true;
                break;
            }

            if (revieved_length > 0 && (!cid_found))
            {
                // Test oposite casing variant of the APN name.
                char oposite_case[64];
                memcpy(oposite_case, apn, strlen(apn));

                if (apn[0] >= 'a')
                {
                    for (uint8_t i = 0; i < strlen(apn); i++)
                    {
                        oposite_case[i] = apn[i] - 32;
                    }
                } else {
                    for (uint8_t i = 0; i < strlen(apn); i++)
                    {
                        oposite_case[i] = apn[i] + 32;
                    }
                }
                oposite_case[strlen(apn)] = '\0';

                if ((revieved_length > 0) && (strstr(read_buffer, oposite_case) != NULL))
                {
                    cid_found = true;
                    break;
                }
            }
        }

        // Clean up CID lookup socket.
        close(at_socket_fd);
    }

    // Now block until IPv6 is ready to be used.
    if (cid_found)
    {
        bool ipv6_active = false;
        do
        {
            memset(read_buffer, 0, sizeof(read_buffer));
            received_cgev_length = recv(at_socket_cgev_fd, read_buffer, sizeof(read_buffer), 0);
            if ((received_cgev_length > 0) && (strstr(read_buffer, "CGEV: IPV6") != NULL))
            {
                // We have a hit on IPV6 link up notification, lets verify the CID.
                char * p_start = strstr(read_buffer, "IPV6");
                int cid_candidate = atoi(&p_start[strlen("IPV6") + 1]);
                if (cid_candidate == cid_number)
                {
                    ipv6_active = true;
                }
            }
        } while (!ipv6_active);
    }

    close(at_socket_cgev_fd);

    return apn_handle;
}


int at_read_imei_and_msisdn(char *p_imei, int imei_len, char *p_msisdn, int msisdn_len)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;
    int retval = 0;

    if (imei_len < 15 || msisdn_len < 10) {
        return EINVAL;
    }

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return EIO;
    }

    // Read IMEI
    const char *at_cgsn = "AT+CGSN";
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_cgsn);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        memset(p_imei, 0, imei_len);
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            memcpy(p_imei, read_buffer, 15);
        } else {
            printk("recv(%s) failed\n", at_cgsn);
            retval = EIO;
        }
    } else {
        printk("send(%s) failed\n", at_cgsn);
        retval = EIO;
    }

    // Read MSISDN
    const char *at_cnum = "AT+CNUM";
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_cnum);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        memset(p_msisdn, 0, msisdn_len);
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            char * p_start = strstr(read_buffer, "\"");
            if (p_start) {
                char * p_end = strstr(p_start + 1, "\"");
                if (p_end && (p_end - p_start - 1 >= 10)) {
                    // FIXME: This only uses the last 10 digits
                    memcpy(p_msisdn, p_end - 10, 10);
                }
            }
            if (!p_msisdn[0]) {
                // SIM has no number
                // FIXME: For debug purpose use the last 10 digits of IMEI
                memcpy(p_msisdn, &p_imei[5], 10);
            }
        } else {
            printk("recv(%s) failed\n", at_cnum);
            retval = EIO;
        }
    } else {
        printk("send(%s) failed\n", at_cnum);
        retval = EIO;
    }

    close(at_socket_fd);

    return retval;
}

static int copy_and_convert_iccid(const char *src, uint32_t src_len, char *dst, uint32_t *p_dst_len)
{
    if (*p_dst_len < src_len) {
        return EINVAL;
    }

    int len = 0;

    // https://www.etsi.org/deliver/etsi_ts/102200_102299/102221/13.02.00_60/ts_102221v130200p.pdf chapter 13.2
    for (int i = 0; i < src_len; i += 2) {
        dst[len++] = src[i + 1];
        if (src[i] != 'F') {
            dst[len++] = src[i];
        }
    }

    *p_dst_len = len;

    return 0;
}

int at_read_sim_iccid(char *p_iccid, uint32_t * p_iccid_len)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;
    int retval = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return EIO;
    }

    // Read SIM ICCID
    const char *at_crsm = "AT+CRSM=176,12258,0,0,10";
    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_crsm);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            char * p_start = strstr(read_buffer, "\"");
            if (p_start) {
                char * p_end = strstr(p_start + 1, "\"");
                if (p_end) {
                    retval = copy_and_convert_iccid(p_start + 1, p_end - p_start - 1, p_iccid, p_iccid_len);
                }
            }
        } else {
            printk("recv(%s) failed\n", at_crsm);
            retval = EIO;
        }
    } else {
        printk("send(%s) failed\n", at_crsm);
        retval = EIO;
    }

    close(at_socket_fd);

    return retval;
}

int at_read_firmware_version(char *p_fw_version, uint32_t *p_fw_version_len)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;
    int retval = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return EIO;
    }

    // Read Modem version
    const char at_cgmr[] = "AT+CGMR";
    length = send(at_socket_fd, at_cgmr, sizeof(at_cgmr) - 1, 0);

    if (length == sizeof(at_cgmr) - 1) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            char * p_end = strstr(read_buffer, "\r");
            if (p_end) {
                uint32_t len = (uint32_t)(p_end - read_buffer);
                memcpy(p_fw_version, read_buffer, len);
                p_fw_version[len] = '\0';
                *p_fw_version_len = len;
            } else {
                retval = EINVAL;
            }
        } else {
            printk("recv(%s) failed\n", at_cgmr);
            retval = EIO;
        }
    } else {
        printk("send(%s) failed\n", at_cgmr);
        retval = EIO;
    }

    close(at_socket_fd);

    return retval;
}

int at_send_command(const char *at_command, bool do_logging)
{
    char write_buffer[APP_MAX_AT_WRITE_LENGTH];
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int at_socket_fd;
    int length;
    int retval = 0;

    at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
    if (at_socket_fd < 0) {
        printk("socket() failed\n");
        return EIO;
    }

    snprintf(write_buffer, APP_MAX_AT_WRITE_LENGTH, "%s", at_command);
    length = send(at_socket_fd, write_buffer, strlen(write_buffer), 0);

    if (length == strlen(write_buffer)) {
        length = recv(at_socket_fd, read_buffer, APP_MAX_AT_READ_LENGTH, 0);
        if (length > 0) {
            if (do_logging) {
                printk("%s", read_buffer);
            }
        } else {
            printk("recv() failed\n");
            retval = EIO;
        }
    } else {
        printk("send() failed\n");
        retval = EIO;
    }

    close(at_socket_fd);

    return retval;
}
