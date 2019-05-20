/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <net/socket.h>

#define APP_MAX_AT_READ_LENGTH          256
#define APP_MAX_AT_WRITE_LENGTH         256

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
