/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <at_interface.h>

#include <stdlib.h>
#include <lwm2m.h>
#include <lwm2m_os.h>
#include <lwm2m_api.h>
#include <nrf_socket.h>
#include <sms_receive.h>

// FIXME: remove this and move to KConfig
#define APP_MAX_AT_READ_LENGTH          CONFIG_AT_CMD_RESPONSE_MAX_LEN
#define APP_MAX_AT_WRITE_LENGTH         256

/** Cumulative days per month in a year
 *  Leap days are taken into account in the formula calculating the time since Epoch.
 *  */
static int cum_ydays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/** @brief PDN context ID. Negative values if no CID found. */
static volatile int8_t cid_number = -1;
static volatile bool cid_ipv6_link_up = false;

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

static int at_send_command_and_parse_params(const char * p_at_command, struct lwm2m_os_at_param_list * p_param_list)
{
    char read_buffer[APP_MAX_AT_READ_LENGTH];

    int retval = 0;

    retval = lwm2m_os_at_cmd_write(p_at_command, read_buffer, APP_MAX_AT_READ_LENGTH);

    if (retval == 0) {
        retval = lwm2m_os_at_parser_params_from_str(read_buffer, NULL, p_param_list);
    }

    return retval;
}

static int at_response_param_to_lwm2m_string(const char * p_at_command, lwm2m_string_t * p_string)
{
    int retval = 0;
    struct lwm2m_os_at_param_list params;

    char read_buf[APP_MAX_AT_READ_LENGTH];
    size_t buf_len = sizeof(read_buf);

    if (lwm2m_os_at_params_list_init(&params, 1) == 0)
    {
        if (at_send_command_and_parse_params(p_at_command, &params) == 0)
        {
            retval = lwm2m_os_at_params_string_get(&params, 0, read_buf, &buf_len);
            if (retval == 0)
            {
                (void)lwm2m_bytebuffer_to_string(read_buf, buf_len, p_string);
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

        lwm2m_os_at_params_list_free(&params);
    }
    else
    {
        LWM2M_ERR("at_params_list_init failed");
        retval = -EINVAL;
    }

    return retval;
}

static int at_response_param_to_string(const char * p_at_command, int param_count,
                                        int param_idx, char * const p_string_buffer,
                                        size_t *p_buffer_len)
{
    int retval = 0;
    struct lwm2m_os_at_param_list params;

    if (lwm2m_os_at_params_list_init(&params, param_count) == 0)
    {
        int err = at_send_command_and_parse_params(p_at_command, &params);
        if (err == 0 || err == -EAGAIN || err ==-E2BIG)
        {
            err = lwm2m_os_at_params_string_get(&params, param_idx, p_string_buffer, p_buffer_len);
            if (err == 0)
            {
                /* lwm2m_os_at_params_string_get will return an error if tmp_len is not smaller than param len.
                 * No need to check size in this level. */
                p_string_buffer[*p_buffer_len] = '\0';
            }
            else
            {
                LWM2M_ERR("parse failed: no string param found: %d", err);
                retval = -EINVAL;
            }
        }
        else
        {
            retval = err;
        }

        lwm2m_os_at_params_list_free(&params);
    }
    else
    {
        LWM2M_ERR("lwm2m_os_at_params_list_init failed");
        retval = -EINVAL;
    }

    return retval;
}

static void at_response_handler(void *context, char *response)
{
    ARG_UNUSED(context);

    for (int i = 0; i < ARRAY_SIZE(at_handlers); i++) {
        int ret = at_handlers[i](response);
        if (ret == 0) {
            // Message or events is consumed.
            // Skip next handlers and wait for next message/event.
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

        // IPv6 link is up for the default bearer.
        if (strncmp(cgev_evt, "IPV6 ", 5) == 0)
        {
            int timeout_ms = 100; // 100 millisecond timeout.
            while (cid_number == -1 && timeout_ms > 0) {
                // Wait for nrf_getsockopt() to set cid_number.
                 lwm2m_os_sleep(10);
                 timeout_ms -= 10;
            }

            // Match CID with PDN socket context.
            int cid = strtol(&cgev_evt[5], NULL, 0);
            if (cid >= 0 && cid == cid_number)
            {
                cid_ipv6_link_up = true;
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
    struct lwm2m_os_at_param_list cereg_params;

    // Check if this is a CGEV event.
    int length = strlen(notif);
    if (length >= 8 && strncmp(notif, "+CEREG: ", 7) == 0)
    {
        retval = lwm2m_os_at_params_list_init(&cereg_params, 2);

        if (retval == 0) {
            retval = lwm2m_os_at_parser_params_from_str(notif, NULL, &cereg_params);
            if (retval == 0 || retval == -E2BIG)
            {
                uint16_t net_reg_stat;
                if (lwm2m_os_at_params_short_get(&cereg_params, 1, &net_reg_stat) == 0)
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
                LWM2M_ERR("at_parser (%s) failed (%d)", lwm2m_os_log_strdup(notif), retval);
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

int at_if_init(void)
{
    int err;

    err = lwm2m_os_at_init();
    if (err) {
            LWM2M_ERR("Failed to initialize AT interface");
            return -1;
    }

    // Set handler for AT notifications and events (SMS, CESQ, etc.).
    err = lwm2m_os_at_notif_register_handler(NULL, at_response_handler);
    if (err) {
        LWM2M_ERR("Failed to register AT handler");
        return -1;
    }

    return 0;
}

int at_apn_setup_wait_for_ipv6(const char * const apn)
{
    if (apn == NULL) {
        return -1;
    }

    // Clear previous state before registering for packet domain events.
    cid_number = -1;
    cid_ipv6_link_up = false;

    // Register for packet domain event reporting +CGEREP.
    // The unsolicited result code is +CGEV: XXX.
    int err = lwm2m_os_at_cmd_write("AT+CGEREP=1", NULL, 0);

    if (err != 0)
    {
        // Check if subscription went OK.
        LWM2M_ERR("Unable to register to CGEV events for IPv6 APN");
        return -1;
    }

    // Set up APN which implicitly creates a CID.
    int apn_handle = lwm2m_os_pdn_init_and_connect(apn);

    if (apn_handle > -1)
    {
        nrf_socklen_t len = sizeof(cid_number);
        int error = nrf_getsockopt(apn_handle, NRF_SOL_PDN, NRF_SO_PDN_CONTEXT_ID, (void *)&cid_number, &len);

        if (error == 0)
        {
            LWM2M_INF("PDN cid %d found. Wait for IPv6 link...", cid_number);

            int timeout_ms = 1 * 60 * 1000; // One minute timeout.

            // Wait until IPv6 link is up or timeout.
            while (cid_ipv6_link_up == false && timeout_ms > 0) {
                lwm2m_os_sleep(100);
                timeout_ms -= 100;
            }

            if (timeout_ms <= 0)
            {
                LWM2M_ERR("Timeout while waiting for IPv6 (cid=%u)", cid_number);
                lwm2m_os_pdn_disconnect(apn_handle); // Cleanup socket
                apn_handle = -1;
            }
            else
            {
                LWM2M_INF("IPv6 link ready for cid %d", cid_number);
            }
        }
        else
        {
            LWM2M_ERR("Unable to get PDN context ID on socket %d, errno=%d", apn_handle, errno);
            lwm2m_os_pdn_disconnect(apn_handle); // Cleanup socket
            apn_handle = -1;
        }
    }

    // Unregister from packet domain events.
    (void)lwm2m_os_at_cmd_write("AT+CGEREP=0", NULL, 0);

    return apn_handle;
}

int at_read_imei(char * const p_imei, int imei_len)
{
    int retval = 0;

    if (p_imei == NULL || imei_len < 16) {
        return EINVAL;
    }

    // Read IMEI.
    int len = imei_len;
    int err = at_response_param_to_string("AT+CGSN=1", 2, 1, p_imei, &len);

    if (err != 0)
    {
        // AT command error
        LWM2M_ERR("Unable to read IMEI. AT command error %d.", err);
        retval = EIO;
    }

    return retval;
}

int at_read_msisdn(char * const p_msisdn, int msisdn_len)
{
    int retval = 0;

    if (p_msisdn == NULL || msisdn_len < 16) {
        return EINVAL;
    }

    // Read subscriber number (MSISDN).
    // AT command response format: +CNUM: ,"+1234567891234",145 or ERROR.
    int len = msisdn_len;
    int err = at_response_param_to_string("AT+CNUM", 4, 2, p_msisdn, &len);

    if (err == -ENOEXEC) {
        // An ERROR response is returned if MSISDN is not available on SIM card or if SIM card is not initialized.
        LWM2M_ERR("No subscriber number (MSISDN) available on this SIM.");
        retval = EPERM;
    }
    else if(err != 0) {
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
    char string_buffer[APP_MAX_AT_READ_LENGTH];
    int retval = 0;

    if (p_iccid == NULL || *p_iccid_len < 16) {
        return EINVAL;
    }

    // Read SIM Integrated Circuit Card Identifier (ICCID)
    int len = sizeof(string_buffer);
    int err = at_response_param_to_string("AT+CRSM=176,12258,0,0,10", 4, 3, string_buffer, &len);
    if (err == 0) {
        retval = copy_and_convert_iccid(string_buffer, len - 1, p_iccid, p_iccid_len);
    } else {
        LWM2M_ERR("Unable to read ICCID. AT command error.");
        retval = EIO;
    }

    return retval;
}

int at_read_firmware_version(lwm2m_string_t *p_manufacturer_id)
{
    int retval = 0;

    // Read manufacturer identification
    const char *at_cgmi = "AT+CGMR";

    retval = at_response_param_to_lwm2m_string(at_cgmi, p_manufacturer_id);

    return retval;
}

int at_read_operator_id(uint32_t  *p_operator_id)
{
    int retval = 0;
    struct lwm2m_os_at_param_list operid_params;

    *p_operator_id = 0;

    // Read network registration status
    const char *at_operid = "AT%XOPERID";

    if (lwm2m_os_at_params_list_init(&operid_params, 2))
    {
        LWM2M_ERR("operid_params list init failed");
        retval = EINVAL;
    }
    else
    {
        int ret = at_send_command_and_parse_params(at_operid, &operid_params);
        if (ret == 0)
        {
            uint32_t operator_id;
            if (lwm2m_os_at_params_int_get(&operid_params, 1, &operator_id) == 0)
            {
                *p_operator_id = (uint32_t)operator_id;
            }
            else
            {
                LWM2M_ERR("operator id parse failed: get int failed");
                retval = EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("parse operator id failed");
            retval = EIO;
        }

        lwm2m_os_at_params_list_free(&operid_params);
    }

    return retval;
}

void at_subscribe_net_reg_stat(at_net_reg_stat_cb_t net_reg_stat_cb)
{
    int retval = 0;

    m_net_reg_stat_cb = net_reg_stat_cb;

    retval = lwm2m_os_at_cmd_write("AT+CEREG=2", NULL, 0);

    if (retval != 0) {
        LWM2M_ERR("AT+CEREG=2 failed: %d", (int)retval);
    }
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
    struct lwm2m_os_at_param_list cesq_params;

    // Read network registration status
    const char *at_cesq = "AT+CESQ";

    *p_signal_strength = 0;
    *p_link_quality = 0;

    retval = lwm2m_os_at_params_list_init(&cesq_params, 7);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_cesq, &cesq_params) == 0)
        {
            // Radio signal strength
            uint32_t rsrp;
            if (lwm2m_os_at_params_int_get(&cesq_params, 6, &rsrp) == 0)
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
                    *p_signal_strength = -141 + (int32_t)rsrp;
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
            uint16_t rsrq;
            if (lwm2m_os_at_params_short_get(&cesq_params, 5, &rsrq) == 0)
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

        lwm2m_os_at_params_list_free(&cesq_params);
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
    char ci_buf[9];

    *p_cell_id = 0;

    // Read network registration status
    int len = sizeof(ci_buf);
    int err = at_response_param_to_string("AT+CEREG?", 6, 4, ci_buf, &len);

    if (err == 0)
    {
        *p_cell_id = (uint32_t)strtol(ci_buf, NULL, 16);
    }
    else
    {
        LWM2M_ERR("Reading cell id failed");
        retval = -EIO;
    }

    return retval;
}

int at_read_smnc_smcc(int32_t * p_smnc, int32_t *p_smcc)
{
    int retval = 0;
    char oper_buf[8];

    *p_smnc = 0;
    *p_smcc = 0;

    // Read network registration status
    int len = sizeof(oper_buf);
    int err = at_response_param_to_string("AT+COPS?", 5, 3, oper_buf, &len);

    if (err == 0)
    {
        // SMNC is first 3 characters, SMNN the following characters
        *p_smcc = (int32_t)strtol(&oper_buf[3], NULL, 0);
        oper_buf[3] = '\0'; // Null termination of SMNC
        *p_smnc = (int32_t)strtol(oper_buf, NULL, 0);
    }
    else
    {
        LWM2M_ERR("Reading smnc & smcc failed: %d", err);
        retval = -EIO;
    }

    return retval;
}

int at_read_time(int32_t *p_time, int32_t *p_utc_offset, int32_t *p_dst_adjustment)
{
    int retval = 0;
    char string_buf[21];
    int len = sizeof(string_buf);

    *p_time = 0;
    *p_utc_offset = 0;
    *p_dst_adjustment = 0;

    // Read modem time with DST
    struct lwm2m_os_at_param_list cclk_params;

    int err = lwm2m_os_at_params_list_init(&cclk_params, 3);
    if (err == 0)
    {
        err = at_send_command_and_parse_params("AT%CCLK?", &cclk_params);
        if (err == -ENOEXEC)
        {
            err = at_send_command_and_parse_params("AT+CCLK?", &cclk_params);
        }

        // Get time string
        if (err == 0)
        {
            err = lwm2m_os_at_params_string_get(&cclk_params, 1, string_buf, &len);
            if (err == 0)
            {
                string_buf[len] = '\0';
                at_cclk_reponse_convert(string_buf, p_time, p_utc_offset);
            }
        }

        // Get DST if available
        if (err == 0 && lwm2m_os_at_params_valid_count_get(&cclk_params) == 3)
        {
            uint32_t dst_hrs;
            err = lwm2m_os_at_params_int_get(&cclk_params, 2, &dst_hrs);
            if (err == 0)
            {
                *p_dst_adjustment = (int32_t)dst_hrs;
            }
        }

        if (err == -ENOEXEC)
        {
            // Note: reading modem time can also fail because network time is not yet available
            LWM2M_INF("Modem time not available");
        }
        else if (err != 0)
        {
            LWM2M_ERR("Reading modem time failed: %d", err);
            retval = -EIO;
        }
    }
    else
    {
        LWM2M_ERR("cclk_params list init failed: %d", err);
        retval = -EINVAL;
    }

    return retval;
}

int at_read_ipaddr(lwm2m_list_t * p_ipaddr_list)
{
    char string_buf[APP_MAX_AT_READ_LENGTH];

    int retval = 0;

    // Read IP addresses
    int len = sizeof(string_buf);
    int err = at_response_param_to_string("AT+CGDCONT?", 7, 4, string_buf, &len);

    if (err == 0 || err == -EAGAIN) {
        int idx = 0;
        if (len > 0)
        {
            for (char *ip_addr = strtok(string_buf, " "); ip_addr; ip_addr = strtok(NULL, " ")) {
                if (idx < p_ipaddr_list->max_len) {
                    (void)lwm2m_bytebuffer_to_string(ip_addr, strlen(ip_addr), &p_ipaddr_list->val.p_string[idx++]);
                    p_ipaddr_list->len = idx;
                } else {
                    LWM2M_ERR("ipaddr list full");
                    retval = -ENOMEM;
                    break;
                }
            }
        }
        else
        {
            LWM2M_ERR("IP address string length %d", len);
            retval = -EINVAL;
        }
    }
    else
    {
        LWM2M_ERR("Reading IP addresses failed: %d", err);
        retval = -EIO;
    }

    return retval;
}

int at_read_connstat(lwm2m_connectivity_statistics_t * p_conn_stat)
{
    struct lwm2m_os_at_param_list xconnstat_params;
    int retval;

    const char *at_xconnstat = "AT%XCONNSTAT?";

    retval = lwm2m_os_at_params_list_init(&xconnstat_params, 7);
    if (retval == 0)
    {
        if (at_send_command_and_parse_params(at_xconnstat, &xconnstat_params) == 0)
        {
            if ((lwm2m_os_at_params_int_get(&xconnstat_params, 1, &p_conn_stat->sms_tx_counter) != 0) ||
                (lwm2m_os_at_params_int_get(&xconnstat_params, 2, &p_conn_stat->sms_rx_counter) != 0) ||
                (lwm2m_os_at_params_int_get(&xconnstat_params, 3, &p_conn_stat->tx_data) != 0) ||
                (lwm2m_os_at_params_int_get(&xconnstat_params, 4, &p_conn_stat->rx_data) != 0) ||
                (lwm2m_os_at_params_int_get(&xconnstat_params, 5, &p_conn_stat->max_message_size) != 0) ||
                (lwm2m_os_at_params_int_get(&xconnstat_params, 6, &p_conn_stat->average_message_size) != 0))
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

        lwm2m_os_at_params_list_free(&xconnstat_params);
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
    return lwm2m_os_at_cmd_write("AT%XCONNSTAT=1", NULL, 0);
}

int at_stop_connstat(void)
{
    return lwm2m_os_at_cmd_write("AT%XCONNSTAT=0", NULL, 0);
}
