/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include <at_interface.h>

#include <net/socket.h>
#include <pdn_management.h>
#include <sms_receive.h>
#include <at_cmd.h>
#include <at_cmd_parser.h>
#include <at_params.h>
#include <lwm2m_api.h>

/* For logging API. */
#include <lwm2m.h>

// FIXME: remove this and move to KConfig
#define APP_MAX_AT_READ_LENGTH          CONFIG_AT_CMD_RESPONSE_MAX_LEN
#define APP_MAX_AT_WRITE_LENGTH         256

/** Cumulative days per month in a year
 *  Leap days are taken into account in the formula calculating the time since Epoch.
 *  */
static int cum_ydays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/**
 * @brief The array index is the CID number.
 *
 * The maximum CIDs allowed in the system defines the size of the table.
 * The table index is the CID value (CID value 0 to 11).
 */
static volatile bool cid_ipv6_table[12];

/**
 * @brief At command events or notifications handler.
 *
 * @param[in] evt Pointer to a null-terminated AT string (notification or event).
 * @return 0 if the event has been consumed,
 *           or an error code if the event should be propagated to the other handlers.
 */
typedef int (*at_notif_handler)(char* evt);

static int at_cgev_handler(char *notif);
static int at_cereg_handler(char *notif);

static const at_notif_handler at_handlers[] = {
    at_cgev_handler,          ///< Parse AT CGEV events for PDN/IPv6.
    sms_receiver_notif_parse, ///< Parse received SMS events.
    at_cereg_handler          ///< Parse AT CEREG events
};

at_net_reg_stat_cb_t m_net_reg_stat_cb;

static int at_send_command_and_parse_params(const char * p_at_command, struct at_param_list * p_param_list)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int retval = 0;

    retval = at_cmd_write(p_at_command, read_buffer, APP_MAX_AT_READ_LENGTH, NULL);

    if (retval == 0) {
        char * p_start = read_buffer;
        char * p_command_end = strchr(read_buffer, ':');
        if (p_command_end) {
            p_start = p_command_end+1;
        }
        retval = at_parser_params_from_str(p_start, p_param_list);
        if (retval != 0)
        {
            LWM2M_ERR("at_parser (%s) failed", lwm2m_os_log_strdup(p_at_command));
            retval = EINVAL;
        }
    } else {
        LWM2M_ERR("at_cmd_write failed: %d", (int)retval);
    }

    return retval;
}

static int at_response_param_to_lwm2m_string(const char * p_at_command, lwm2m_string_t *p_string)
{
    int retval = 0;
    struct at_param_list params;

    char read_buf[APP_MAX_AT_READ_LENGTH];

    params.params = NULL;
    if (at_params_list_init(&params, 1) == 0)
    {
        if (at_send_command_and_parse_params(p_at_command, &params) == 0)
        {
            retval = at_params_string_get(&params, 0, read_buf, sizeof(read_buf));
            if (retval >= 0)
            {
                (void)lwm2m_bytebuffer_to_string(read_buf, retval, p_string);
            }
            else
            {
                LWM2M_ERR("parse failed: no string param found");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("at_send_command_and_parse_params failed");
            retval = -EIO;
        }

        at_params_list_free(&params);
    }
    else
    {
        LWM2M_ERR("at_params_list_init failed");
        retval = -EINVAL;
    }

    return retval;
}

static void at_response_handler(char *response)
{
    for (int i = 0; i < ARRAY_SIZE(at_handlers); i++) {
        int ret = at_handlers[i](response);
        if (ret == 0) {
            // Message or events is consumed. Skip next handlers and wait for next message/event.
            return;
        }
    }
}

static int at_cgev_handler(char *notif)
{
    // Check if this is a CGEV event.
    int length = strlen(notif);
    if (length >= 8 && strncmp(notif, "+CGEV: ", 7) == 0)
    {
        // Check type of CGEV event.
        char * cgev_evt = &notif[7];

        // TODO: Check if IPV6 fail for CID first.

        // IPv6 link is up for the default bearer.
        if (strncmp(cgev_evt, "IPV6 ", 5) == 0)
        {
            // Save result for each CIDs.
            int cid = strtol(&cgev_evt[5], NULL, 0);
            if (cid >= 0 && cid < ARRAY_SIZE(cid_ipv6_table))
            {
                cid_ipv6_table[cid] = true;
            }
        }

        // CGEV event parsed.
        return 0;
    }

    // Not a CGEV event.
    return -1;
}

static int at_cereg_handler(char *notif)
{
    int retval = 0;
    struct at_param_list cereg_params;

    // Check if this is a CGEV event.
    int length = strlen(notif);
    if (length >= 8 && strncmp(notif, "+CEREG: ", 7) == 0)
    {
        cereg_params.params = NULL;
        retval = at_params_list_init(&cereg_params, 2);

        if (retval == 0) {
            char * p_start = notif;
            char * p_command_end = strchr(notif, ':');
            if (p_command_end) {
                p_start = p_command_end+1;
            }
            retval = at_parser_params_from_str(p_start, &cereg_params);
            if (retval == 0)
            {
                u16_t net_reg_stat;
                if (at_params_short_get(&cereg_params, 0, &net_reg_stat) == 0)
                {
                    if (m_net_reg_stat_cb != NULL)
                    {
                        m_net_reg_stat_cb((uint32_t)net_reg_stat);
                    }
                    else
                    {
                        LWM2M_ERR("No net stat cb");
                    }
                }
                else
                {
                    LWM2M_ERR("failed to get net stat (%s)", lwm2m_os_log_strdup(notif));
                }
            }
            else
            {
                LWM2M_ERR("at_parser (%s) failed", lwm2m_os_log_strdup(notif));
                retval = EINVAL;
            }
        } else {
            LWM2M_ERR("cereg param list init failed: %d", (int)retval);
        }

        // CEREG event parsed.
        return 0;
    }

    // Not a CEREG event.
    return -1;
}

/**@brief Convert AT+CCLK? at command response into seconds since Epoch and UTC offset.
 *
 * @param[in]  p_read_buf   Pointer to response string
 * @param[out] p_time       Pointer to time since Epoch
 * @param[out] p_utc_offset Pointer to UTC offset
 *
 * */
static void at_cclk_reponse_convert(const char *p_read_buf, int32_t *p_time, int32_t *p_utc_offset)
{
    // Seconds since Epoch approximation
    char *p_end;
    int tmp_year = 2000 + (int32_t)strtol(p_read_buf, &p_end, 10);
    int year = tmp_year - 1900;
    int mon = (int32_t)strtol(p_end + 1, &p_end, 10) - 1;
    int mday = (int32_t)strtol(p_end + 1, &p_end, 10);
    int hour = (int32_t)strtol(p_end + 1, &p_end, 10);
    int min = (int32_t)strtol(p_end + 1, &p_end, 10);
    int sec = (int32_t)strtol(p_end + 1, &p_end, 10);

    if ((mon > 11) || (mon < 0)) {
        mon = 0;
    }

    int yday = mday - 1 + cum_ydays[mon];

    /*
     * The Open Group Base Specifications Issue 7, 2018 edition
     * IEEE Std 1003.1-2017: 4.16 Seconds Since the Epoch
     *
     * http://http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_16
     */
    *p_time = sec + min * 60 + hour * 3600 + yday * 86400 +
          (year - 70) * 31536000 + ((year - 69) / 4) * 86400 -
          ((year - 1) / 100) * 86400 + ((year + 299) / 400) * 86400;

    // UTC offset as 15 min units
    *p_utc_offset = (int32_t)strtol(p_end, &p_end, 10);
}

int mdm_interface_init(void)
{
    // The AT command driver initialization is done automatically by the OS.
    // Set handler for AT notifications and events (SMS, CESQ, etc.).
    at_cmd_set_notification_handler(at_response_handler);

    LWM2M_INF("Modem interface initialized.");
    return 0;
}

int at_apn_setup_wait_for_ipv6(char * apn)
{
    int apn_handle = -1;
    int cid_number = -1;
    bool cid_found = false;

    if (apn == NULL) {
        return -1;
    }

    // Clear the CID table before registering for packet events.
    memset((void *)cid_ipv6_table, 0x00, sizeof(cid_ipv6_table));

    // Register for packet domain event reporting +CGEREP.
    // The unsolicited result code is +CGEV: XXX.
    int err = at_cmd_write("AT+CGEREP=1", NULL, 0, NULL);

    if (err != 0)
    {
        // Check if subscription went OK.
        LWM2M_ERR("Unable to register to CGEV events for IPv6 APN");
        return -1;
    }

    // Set up APN which implicitly creates a CID.
    apn_handle = pdn_init_and_connect(apn);

    if (apn_handle > -1)
    {
        char at_cmd[16];
        char read_buffer[APP_MAX_AT_READ_LENGTH];

        // Loop through all possible CID values and search for the APN.
        for (cid_number = 0; cid_number < ARRAY_SIZE(cid_ipv6_table); cid_number++) {
            snprintf(at_cmd, sizeof(at_cmd), "AT+CGCONTRDP=%u", cid_number);

            int err = at_cmd_write(at_cmd, read_buffer, sizeof(read_buffer), NULL);

            if (err != 0)
            {
                LWM2M_ERR("Unable to read information for PDN connection (cid=%u)", cid_number);
                break;
            }

            // TODO: Use parser instead of strstr. Change only APN name in response to lower case.
            if (strstr(read_buffer, apn) != NULL)
            {
                // APN name found in the AT command response for CID.
                cid_found = true;
                break;
            }

            if (cid_found == false)
            {
                // Test opposite casing variant of the APN name.
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

                if (strstr(read_buffer, oposite_case) != NULL)
                {
                    // APN name found in the AT command response for CID.
                    cid_found = true;
                    break;
                }
            }
        }
    }

    // Block until IPv6 is ready to be used or timeout after 5 minutes.
    if (cid_found)
    {
        LWM2M_TRC("CID %d found", cid_number);

        int timeout = 5 * 60 * 1000; // 5 minutes timeout
        while (cid_ipv6_table[cid_number] == false && timeout > 0) {
            lwm2m_os_sleep(100);
            timeout -= 100;
        }

        if (timeout == 0) {
            LWM2M_ERR("Timeout waiting for IPv6 (cid=%u)", cid_number);
            pdn_disconnect(apn_handle);
            apn_handle = -1;
        } else {
            LWM2M_TRC("IPv6 available for CID %d", cid_number);
        }
    }

    // Do not forward unsolicited CGEV result codes. Not needed anymore.
    (void)at_cmd_write("AT+CGEREP=0", NULL, 0, NULL);

    return apn_handle;
}

int at_read_imei(char * const p_imei, int imei_len)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];
    int retval = 0;

    if (p_imei == NULL || imei_len < 16) {
        return EINVAL;
    }

    // Read IMEI.
    // AT response format: 490154203237518\r\n or ERROR
    int err = at_cmd_write("AT+CGSN", read_buffer, APP_MAX_AT_READ_LENGTH, NULL);
    if (err == 0) {
        // TODO: Use AT+CGSN=1 instead and parser
        // OK response from the Modem. Extract IMEI from response payload (15 digits).
        memcpy(p_imei, read_buffer, 15);
        p_imei[15] = '\0';
    }
    else {
        // AT command error
        LWM2M_ERR("Unable to read IMEI. AT command error %d.", err);
        retval = EIO;
    }

    return retval;
}

int at_read_msisdn(char * const p_msisdn, int msisdn_len)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];
    int retval = 0;

    if (p_msisdn == NULL || msisdn_len < 16) {
        return EINVAL;
    }

    // Read subscriber number (MSISDN).
    // AT command response format: +CNUM: ,"+1234567891234",145 or ERROR.
    int err = at_cmd_write("AT+CNUM", read_buffer, APP_MAX_AT_READ_LENGTH, NULL);

    if(err == 0) {
        // OK response from the Modem. Parse CNUM response.
        // TODO: Use AT parser instead
        char * p_start = strstr(read_buffer, "\"");
        char * p_end = NULL;

        if (p_start) {
            p_start += 1;
            p_end = strstr(p_start, "\"");
            if (p_end) {
                // MSISDN is max 15 digits. May be less depending on the operator.
                int len = p_end - p_start;
                memcpy(p_msisdn, p_start, len);
                p_msisdn[len] = '\0';
            }
        }

        if (p_start == NULL || p_end == NULL) {
            // Invalid AT command response format.
            LWM2M_ERR("Invalid MSISDN format.");
            retval = EIO;
        }
    }
    else if (err == -ENOEXEC) {
        // An ERROR response is returned if MSISDN is not available on SIM card or if SIM card is not initialized.
        LWM2M_ERR("No subscriber number (MSISDN) available on this SIM.");
        retval = EPERM;
    }
    else {
        // Unknown error.
        LWM2M_ERR("Unable to read MSISDN. AT command error %d.", err);
        retval = EIO;
    }

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
    char read_buffer[APP_MAX_AT_READ_LENGTH];
    int retval = EIO;

    if (p_iccid == NULL || p_iccid_len == NULL || *p_iccid_len < 20) {
        return EINVAL;
    }

    // Read SIM Integrated Circuit Card Identifier (ICCID)
    int err = at_cmd_write("AT+CRSM=176,12258,0,0,10", read_buffer, APP_MAX_AT_READ_LENGTH, NULL);
    if (err == 0) {
        // OK response from the Modem. Extract and convert the ICCID.
        // +CRSM: 144,0,\"981380398101342451F7\"\r\n
        char * p_start = strstr(read_buffer, "\"");
        if (p_start) {
            char * p_end = strstr(p_start + 1, "\"");
            if (p_end) {
                retval = copy_and_convert_iccid(p_start + 1, p_end - p_start - 1, p_iccid, p_iccid_len);
            }
        }
    }

    if (retval != 0) {
        // AT command read error or invalid AT response format
        LWM2M_ERR("Unable to read ICCID. AT command error.");
        retval = EIO;
    }

    return retval;
}

int at_read_firmware_version(char *p_fw_version, uint32_t *p_fw_version_len)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];
    int retval = EINVAL;

    if (p_fw_version == NULL || p_fw_version_len == NULL) {
        return EINVAL;
    }

    // Read Modem revision identification.
    int err = at_cmd_write("AT+CGMR", read_buffer, APP_MAX_AT_READ_LENGTH, NULL);
    if (err == 0) {
        // OK response from the Modem. Extract revision identification.
        // TODO: Use AT parser
        char * p_end = strstr(read_buffer, "\r");
        if (p_end) {
            uint32_t len = (uint32_t)(p_end - read_buffer);
            if(*p_fw_version_len > len) {
                memcpy(p_fw_version, read_buffer, len);
                p_fw_version[len] = '\0';
                *p_fw_version_len = len;
                retval = 0;
            }
            else {
                LWM2M_ERR("Revision identification too big.");
            }
        }
        else {
             LWM2M_ERR("Invalid revision identification format.");
        }
    }
    else {
        LWM2M_ERR("Unable to read firmware version. AT command error %d.", err);
        retval = EIO;
    }

    return retval;
}

int at_read_operator_id(uint32_t  *p_operator_id)
{
    int retval = 0;
    struct at_param_list operid_params;
    uint16_t operator_id;

    *p_operator_id = 0;

    // Read network registration status
    const char *at_operid = "AT%XOPERID";

    operid_params.params = NULL;
    if (at_params_list_init(&operid_params, 1))
    {
        LWM2M_ERR("operid_params list init failed");
        retval = EINVAL;
    }
    else
    {
        if (at_send_command_and_parse_params(at_operid, &operid_params) == 0)
        {
            if (at_params_short_get(&operid_params, 0, &operator_id) == 0)
            {
                *p_operator_id = (uint32_t)operator_id;
            }
            else
            {
                LWM2M_ERR("operator id parse failed: get short failed");
                retval = EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("parse operator id failed");
            retval = EIO;
        }

        at_params_list_free(&operid_params);
    }

    return retval;
}

void at_subscribe_net_reg_stat(at_net_reg_stat_cb_t net_reg_stat_cb)
{
    int retval = 0;

    m_net_reg_stat_cb = net_reg_stat_cb;

    retval = at_cmd_write("AT+CEREG=2", NULL, 0, NULL);

    if (retval != 0) {
        LWM2M_ERR("AT+CEREG=2 failed: %d", (int)retval);
    }
}

int at_read_net_reg_stat(uint32_t * p_net_stat)
{
    int retval = 0;
    struct at_param_list cereg_params;

    // Read network registration status
    const char *at_cereg = "AT+CEREG?";

    cereg_params.params = NULL;
    retval = at_params_list_init(&cereg_params,2);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_cereg, &cereg_params) == 0)
        {
            u16_t net_stat;
            if (at_params_short_get(&cereg_params, 1, &net_stat) == 0)
            {
                *p_net_stat = (uint32_t)net_stat;
            }
            else
            {
                LWM2M_ERR("net stat parsing failed: no short param found");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("reading cereg failed\n");
            retval = -EINVAL;
        }

        at_params_list_free(&cereg_params);
    }
    else
    {
        LWM2M_ERR("cereg params_list_init failed");
        retval = EINVAL;
    }

    return retval;
}

int at_read_manufacturer(lwm2m_string_t *p_manufacturer_id)
{
    int retval = 0;

    // Read manufacturer identification
    const char *at_cgmi = "AT+CGMI";

    retval = at_response_param_to_lwm2m_string(at_cgmi, p_manufacturer_id);

    return retval;
}

int at_read_model_number(lwm2m_string_t *p_model_number)
{
    int retval = 0;

    // Read model number
    const char *at_cgmm = "AT+CGMM";

    retval = at_response_param_to_lwm2m_string(at_cgmm, p_model_number);

    return retval;
}

int at_read_radio_signal_strength_and_link_quality(int32_t * p_signal_strength, int32_t * p_link_quality)
{
    int retval = 0;
    struct at_param_list cesq_params;

    // Read network registration status
    const char *at_cesq = "AT+CESQ";

    *p_signal_strength = 0;
    *p_link_quality = 0;

    cesq_params.params = NULL;

    retval = at_params_list_init(&cesq_params,6);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_cesq, &cesq_params) == 0)
        {
            // Radio signal strength
            u16_t rsrp;
            if (at_params_short_get(&cesq_params, 5, &rsrp) == 0)
            {
                if (rsrp != 255)
                {
                    /*
                     * 3GPP TS 136.133: SI-RSRP measurement report mapping
                     *  Reported value   Measured quantity value   Unit
                     *   CSI_RSRP_00        CSI_RSRP < -140        dBm
                     *   CSI_RSRP_01    -140 =< CSI_RSRP < -139    dBm
                     *   CSI_RSRP_02    -139 =< CSI_RSRP < -138    dBm
                     *   ...
                     *   ...
                     *   ...
                     *   CSI_RSRP_95     -46 =< CSI_RSRP < -45     dBm
                     *   CSI_RSRP_96     -45 =< CSI_RSRP < -44     dBm
                     *   CSI_RSRP_97     -44 =< CSI_RSRP           dBm
                     */
                    *p_signal_strength = -141 + rsrp;
                }
                else
                {
                    // 255 == Not known or not detectable
                    retval = -EINVAL;
                }
            }
            else
            {
                LWM2M_ERR("signal strength parse failed");
                retval = -EINVAL;
            }

            // Link Quality
            u16_t rsrq;
            if (at_params_short_get(&cesq_params, 4, &rsrq) == 0)
            {
                if (rsrq != 255)
                {
                    /*
                     * 3GPP TS 136.133: RSRQ measurement report mapping
                     *  Reported value   Measured quantity value   Unit
                     *   RSRQ_-30            RSRQ < -34             dB
                     *   RSRQ_-29        -34   =< RSRQ < -35.5      dB
                     *   ...
                     *   RSRQ_-02        -20.5 =< RSRQ < -20        dB
                     *   RSRQ_-01        -20   =< RSRQ < -19.5      dB
                     *   RSRQ_00             RSRQ < -19.5           dB
                     *   RSRQ_01        -19.5 =< RSRQ < -19         dB
                     *   RSRQ_02        -19   =< RSRQ < -18.5       dB
                     *   ...
                     *   RSRQ_32        -4    =< RSRQ < -3.5        dB
                     *   RSRQ_33        -3.5  =< RSRQ < -3          dB
                     *   RSRQ_34              -3 =< RSRQ            dB
                     *   RSRQ_35        -3    =< RSRQ < -2.5        dB
                     *   RSRQ_36        -2.5  =< RSRQ < -2          dB
                     *   ...
                     *   RSRQ_45         2    =< RSRQ < 2.5         dB
                     *   RSRQ_46              2.5  =< RSRQ          dB
                     *
                     *   Note: The ranges from RSRQ_-30 to RSRQ_-01 and
                     *   from RSRQ_35 to RSRQ_46 apply for the UE who
                     *   can support extended RSRQ range
                     *
                     *   Since lwm2m supports only integer value for link quality,
                     *   we store the reported value without mapping it.
                     */
                    *p_link_quality = rsrq;
                }
                else
                {
                    // 255 == Not known or not detectable
                    retval = -EINVAL;
                }
            }
            else
            {
                LWM2M_ERR("link quality parse failed");
                retval = -EINVAL;
            }

        }
        else
        {
            LWM2M_ERR("reading cesq failed");
            retval = -EIO;
        }

        at_params_list_free(&cesq_params);
    }
    else
    {
        LWM2M_ERR("cesq_params init failed");
        retval = EINVAL;
    }

    return retval;
}

int at_read_cell_id(uint32_t * p_cell_id)
{
    int retval = 0;
    struct at_param_list cereg_params;
    char ci_buf[9];

    // Read network registration status
    const char *at_cereg = "AT+CEREG?";

    *p_cell_id = 0;

    cereg_params.params = NULL;

    retval = at_params_list_init(&cereg_params, 4);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_cereg, &cereg_params) == 0)
        {
            retval = at_params_string_get(&cereg_params, 3, ci_buf, 8);
            if (retval == 8)
            {
                ci_buf[8] = '\0'; // Null termination
                *p_cell_id = (uint32_t)strtol(ci_buf, NULL, 16);
            }
            else
            {
                LWM2M_ERR("cell_id parse failed: no string param found");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("reading cell id failed");
            retval = -EIO;
        }
        at_params_list_free(&cereg_params);
    }
    else
    {
        LWM2M_ERR("cereg_params list_init failed");
        retval = EINVAL;
    }

    return retval;
}

int at_read_smnc_smcc(int32_t * p_smnc, int32_t *p_smcc)
{
    int retval = 0;
    struct at_param_list cops_params;
    char oper_buf[8];

    // Read network information
    const char *at_cops = "AT+COPS?";

    *p_smnc = 0;
    *p_smcc = 0;

    cops_params.params = NULL;

    retval = at_params_list_init(&cops_params, 4);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_cops, &cops_params) == 0)
        {
            retval = at_params_string_get(&cops_params, 2, oper_buf, sizeof(oper_buf));
            if (retval >= 0)
            {
                    if (retval < sizeof(oper_buf)-1)
                    {
                        // SMNC is first 3 characters, SMNN the following characters
                        oper_buf[retval] = '\0'; // Null termination of SMCC
                        *p_smcc = (int32_t)strtol(&oper_buf[3], NULL, 0);
                        oper_buf[3] = '\0'; // Null termination of SMNC
                        *p_smnc = (int32_t)strtol(oper_buf, NULL, 0);
                    }
                    else
                    {
                        LWM2M_ERR("incorrect cops oper field length");
                        retval = -EINVAL;
                    }
            }
            else
            {
                LWM2M_ERR("cops parse failed: no string param found");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("reading smnc & smcc failed");
            retval = -EIO;
        }
        at_params_list_free(&cops_params);
    }
    else
    {
        LWM2M_ERR("cops_params list_init failed");
        retval = EINVAL;
    }

    return retval;
}

int at_read_time(int32_t *p_time, int32_t *p_utc_offset)
{
    int retval = 0;
    struct at_param_list cclk_params;
    char read_buf[APP_MAX_AT_READ_LENGTH];

    // Read modem time
    const char *at_cclk = "AT+CCLK?";

    *p_time = 0;
    *p_utc_offset = 0;

    cclk_params.params = NULL;

    retval = at_params_list_init(&cclk_params, 1);
    if (retval) {
        LWM2M_ERR("cclk_params list_init failed");
        retval = -EINVAL;
    }else {
        if (at_send_command_and_parse_params(at_cclk, &cclk_params) == 0)
        {
            retval = at_params_string_get(&cclk_params, 0, read_buf, sizeof(read_buf));
            if (retval >= 0)
            {
                if (retval < sizeof(read_buf))
                {
                    // Zero termination
                    read_buf[retval] = '\0';

                    at_cclk_reponse_convert(read_buf, p_time, p_utc_offset);

                    retval = 0;
                }
                else
                {
                    LWM2M_ERR("cclk response too long");
                    retval = -EINVAL;
                }
            }
            else
            {
                LWM2M_ERR("cclk parse failed: no string param found");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("reading time failed");
            retval = -EIO;
        }
        at_params_list_free(&cclk_params);
    }

    return retval;
}

int at_read_ipaddr(lwm2m_list_t * p_ipaddr_list)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];
    struct at_param_list cgdcont_params;
    int retval;

    const char *at_cgdcont = "AT+CGDCONT?";

    retval = at_cmd_write(at_cgdcont, read_buffer, APP_MAX_AT_READ_LENGTH, NULL);

    if (retval == 0) {

        cgdcont_params.params = NULL;
        retval = at_params_list_init(&cgdcont_params, 6);

        if (retval == 0)
        {
            char * p_line_start = read_buffer;
            char * p_command_end = strchr(read_buffer, ':');
            char * p_new_line_pos = strchr(read_buffer, '\n');
            char * p_resp_end = strchr(read_buffer, '\0');

            bool consumed = false;

            int max_ip_addr = p_ipaddr_list->max_len;
            int idx = 0;

            while (consumed != true)
            {
                // Detect if all lines are consumed
                if ((p_resp_end - p_new_line_pos) == 1)
                {
                    consumed = true;
                }

                // Update line start position as next char after echo of the AT command
                if (p_command_end != NULL) {
                    p_line_start = p_command_end+1;
                }
                else
                {
                    LWM2M_ERR("ip addr parsing failed: p_command_end == NULL");
                    retval = -EINVAL;
                    break;
                }

                // Make line a null terminated string for parsing
                read_buffer[p_new_line_pos - read_buffer] = '\0';

                // Parse current line
                retval = at_parser_params_from_str(p_line_start, &cgdcont_params);

                if (retval != 0)
                {
                    LWM2M_ERR("ip addr string parsing failed");
                    retval = -EINVAL;
                    break;
                }

                // Store IP addresses
                int ip_len = at_params_string_get(&cgdcont_params, 3, read_buffer, sizeof(read_buffer));
                read_buffer[ip_len] = '\0';

                for (char *ip_addr = strtok(read_buffer, " "); ip_addr; ip_addr = strtok(NULL, " ")) {
                    if (idx < max_ip_addr) {
                        (void)lwm2m_bytebuffer_to_string(ip_addr, strlen(ip_addr), &p_ipaddr_list->val.p_string[idx++]);
                        p_ipaddr_list->len = idx;
                    } else {
                        LWM2M_ERR("ipaddr list full");
                        at_params_list_free(&cgdcont_params);
                        return -ENOMEM;
                    }
                }

                if (consumed != true)
                {
                    // Set pointers to next line
                    p_command_end = strchr(p_new_line_pos+1, ':');
                    p_new_line_pos = strchr(p_new_line_pos+1, '\n');

                    // Check for error cases
                    if (p_new_line_pos == NULL || p_new_line_pos >= p_resp_end)
                    {
                        LWM2M_ERR("cgdcont parse failed: fault processing multiple lines");
                        retval = -EINVAL;
                        break;
                    }

                    at_params_list_clear(&cgdcont_params);
                }
            }

            at_params_list_free(&cgdcont_params);
        }
        else
        {
            LWM2M_ERR("cgdcont_params list init failed: %d", (int)retval);
        }
    }
    else
    {
        LWM2M_ERR("cgdcont at cmd failed: %d", (int)retval);
    }

    return retval;
}

int at_read_connstat(lwm2m_connectivity_statistics_t * p_conn_stat)
{
    struct at_param_list xconnstat_params;
    int retval;

    const char *at_xconnstat = "AT%XCONNSTAT?";

    xconnstat_params.params = NULL;

    retval = at_params_list_init(&xconnstat_params, 6);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_xconnstat, &xconnstat_params) == 0)
        {
            if ((at_params_int_get(&xconnstat_params, 0, &p_conn_stat->sms_tx_counter) != 0) ||
                (at_params_int_get(&xconnstat_params, 1, &p_conn_stat->sms_rx_counter) != 0) ||
                (at_params_int_get(&xconnstat_params, 2, &p_conn_stat->tx_data) != 0) ||
                (at_params_int_get(&xconnstat_params, 3, &p_conn_stat->rx_data) != 0) ||
                (at_params_int_get(&xconnstat_params, 4, &p_conn_stat->max_message_size) != 0) ||
                (at_params_int_get(&xconnstat_params, 5, &p_conn_stat->average_message_size) != 0))
            {
                LWM2M_ERR("failed to get xconstat");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("at_send_command_and_parse_params failed");
            retval = -EIO;
        }

        at_params_list_free(&xconnstat_params);
    }
    else
    {
        LWM2M_ERR("at_params_list_init failed");
        retval = -EINVAL;
    }

    return retval;
}

int at_start_connstat(void)
{
    return at_cmd_write("AT%XCONNSTAT=1", NULL, 0, NULL);
}

int at_stop_connstat(void)
{
    return at_cmd_write("AT%XCONNSTAT=0", NULL, 0, NULL);
}