/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <at_interface.h>

#include <stdio.h>
#include <stdlib.h>
#include <lwm2m.h>
#include <lwm2m_os.h>
#include <lwm2m_api.h>
#include <lwm2m_pdn.h>
#include <nrf_socket.h>
#include <sms_receive.h>
#include <lwm2m_portfolio.h>

/**
 * @brief Max size for the AT responses.
 */
#define AT_INTERFACE_CMD_RESP_MAX_SIZE 128

#define AT_APN_CLASS_OP_RD "AT%XAPNCLASS=0"
#define AT_APN_CLASS_OP_WR "AT%XAPNCLASS=1"
#define AT_APN_STATUS_OP_RD "AT%XAPNSTATUS?"
#define AT_APN_STATUS_OP_WR "AT%XAPNSTATUS"

enum {
    IPv6_FAIL = -1,
    IPv6_WAIT,
    IPv6_LINK_UP,
};

struct cid_status {
    uint8_t esm_code : 7;
    uint8_t deactive : 1;
};

/** Cumulative days per month in a year.
 *  Leap days are not taken into account.
 *  */
static int cum_ydays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/** @brief PDN context ID. Negative values if no CID found. */
static volatile int8_t cid_number = -1;
static volatile int8_t cid_ipv6_link_up = IPv6_WAIT;

/** @brief ESM error code. */
static volatile struct cid_status esm_error_code[MAX_NUM_OF_PDN_CONTEXTS];
static volatile struct at_restriction restriction_error;

/**
 * @brief At command events or notifications handler.
 *
 * @param[in] evt Pointer to a null-terminated AT string (notification or event).
 * @return 0 if the event has been consumed,
 *           or an error code if the event should be propagated to the other handlers.
 */
typedef int (*at_notif_handler)(const char* evt);

void at_subscribe_odis(void);

static int at_cgev_handler(const char *notif);
static int at_cereg_handler(const char *notif);
static int at_cnec_handler(const char *notif);
static int at_odis_handler(const char *notif);

static const at_notif_handler at_handlers[] = {
    at_cgev_handler,          ///< Parse AT CGEV events for PDN/IPv6.
    sms_receiver_notif_parse, ///< Parse received SMS events.
    at_cereg_handler,         ///< Parse AT CEREG events.
    at_cnec_handler,          ///< Parse AT CNEC events.
    at_odis_handler,          ///< Parse AT ODIS events.
};

at_net_reg_stat_cb_t m_net_reg_stat_cb;

/** Buffer used for at reponse and string params */
static char m_at_buffer[AT_INTERFACE_CMD_RESP_MAX_SIZE];

static int at_send_command_and_parse_params(const char * p_at_command, struct lwm2m_os_at_param_list * p_param_list)
{
    int retval = 0;

    retval = lwm2m_os_at_cmd_write(p_at_command, m_at_buffer, sizeof(m_at_buffer));

    if (retval == 0) {
        retval = lwm2m_os_at_parser_params_from_str(m_at_buffer, NULL, p_param_list);
    }

    return retval;
}

static int at_response_param_to_lwm2m_string(const char * p_at_command, lwm2m_string_t * p_string)
{
    int retval = 0;
    struct lwm2m_os_at_param_list params;

    size_t buf_len = sizeof(m_at_buffer);

    if (lwm2m_os_at_params_list_init(&params, 1) == 0)
    {
        if (at_send_command_and_parse_params(p_at_command, &params) == 0)
        {
            retval = lwm2m_os_at_params_string_get(&params, 0, m_at_buffer, &buf_len);
            if (retval == 0)
            {
                (void)lwm2m_bytebuffer_to_string(m_at_buffer, buf_len, p_string);
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
        LWM2M_ERR("at_params_list_init failed");
        retval = -EINVAL;
    }

    return retval;
}

static void at_response_handler(void *context, const char *response)
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

static int at_cgev_handler(const char *notif)
{
    // Check if this is a CGEV event.
    int length = strlen(notif);
    if (length >= 8 && strncmp(notif, "+CGEV: ", 7) == 0)
    {
        // Check type of CGEV event.
        const char * cgev_evt = &notif[7];

        //LWM2M_INF("CGEV: %s", lwm2m_os_log_strdup(cgev_evt));

        if (strstr(cgev_evt, "PDN DEACT") != NULL) {
            /* AT event: +CGEV: ME/NW PDN DEACT <cid> */
            cgev_evt = strstr(cgev_evt, "DEACT ");
            if (cgev_evt != NULL) {
                int cid = strtol(cgev_evt + 6, NULL, 0);

                if (cid >= 0 && cid < MAX_NUM_OF_PDN_CONTEXTS) {
                    /* PDN deactivated */
                    esm_error_code[cid].deactive = 1;
                }
            }
        } else if (strstr(cgev_evt, "RESTR ") != NULL) {
            /* AT event: +CGEV: RESTR <cause>, <validity>
             *
             * This event is received in case of earlier failure of the PDN.
             * Modem has set the restriction for APN and it cannot be used
             * until throttling timeout is over.
             */
            restriction_error.cause = 0;
            restriction_error.validity = 0;

            cgev_evt = strstr(cgev_evt, "RESTR ");
            if (cgev_evt) {
                restriction_error.cause = strtol(&cgev_evt[6], NULL, 0) & 0xf;
            }
            cgev_evt = strstr(cgev_evt, ", ");
            if (cgev_evt) {
                restriction_error.validity = strtol(&cgev_evt[2], NULL, 0) & 0xf;
            }

        } else if (strncmp(cgev_evt, "IPV6 ", 5) == 0) {

            if (strstr(&cgev_evt[5], "FAIL") == NULL) {
                // IPv6 link is up

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
                    cid_ipv6_link_up = IPv6_LINK_UP;
                }

            } else {
                // IPv6 setup failed
                cid_ipv6_link_up = IPv6_FAIL;
            }
        }

        // CGEV event parsed.
        return 0;
    }

    // Not a CGEV event.
    return -1;
}

static int at_odis_handler(const char *notif)
{
    int retval = 0;
    struct lwm2m_os_at_param_list odis_params;
    lwm2m_portfolio_t *portfolio_inst;

    // Check if this is an ODIS event.
    int len = strlen(notif);
    retval = strncmp(notif, "+ODISNTF: ", 9);

    if (len < 10 || retval != 0)
    {
        // Not an ODIS event.
        return 0;
    }

    retval = lwm2m_os_at_params_list_init(&odis_params, 5);
    if (retval != 0)
    {
        LWM2M_ERR("at_params_list_init failed");
        return 0;
    }

    retval = lwm2m_os_at_parser_params_from_str(notif, NULL, &odis_params);
    if (retval != 0)
    {
        LWM2M_ERR("at_parser_params_from_str failed");
        lwm2m_os_at_params_list_free(&odis_params);
        return 0;
    }

    portfolio_inst = lwm2m_portfolio_get_instance(0);
    if (!portfolio_inst)
    {
        LWM2M_ERR("Primary Host Device Portfolio instance not found");
        lwm2m_os_at_params_list_free(&odis_params);
        return 0;
    }

    for (int i = 1; i < odis_params.param_count; i++)
    {
        len = sizeof(m_at_buffer);
        retval = lwm2m_os_at_params_string_get(&odis_params, i, m_at_buffer, &len);
        if (retval != 0)
        {
            LWM2M_ERR("parse failed: no string param found");
            lwm2m_os_at_params_list_free(&odis_params);
            return 0;
        }

        lwm2m_list_string_set(&portfolio_inst->identity, i - 1, m_at_buffer, len);
    }

    lwm2m_os_at_params_list_free(&odis_params);

    lwm2m_observable_resource_value_changed(LWM2M_OBJ_PORTFOLIO, 0, LWM2M_PORTFOLIO_IDENTITY);

    return 0;
}

static int at_cereg_handler(const char *notif)
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

            lwm2m_os_at_params_list_free(&cereg_params);

        } else {
            LWM2M_ERR("cereg param list init failed: %d", (int)retval);
        }

        // CEREG event parsed.
        return 0;
    }

    // Not a CEREG event.
    return -1;
}

static int at_cnec_handler(const char *notif)
{
    // Check if this is a CNEC event.
    int length = strlen(notif);
    if (length >= 12 && strncmp(notif, "+CNEC_ESM: ", 11) == 0)
    {
        /* AT event: +CNEC_ESM: <cause>,<cid> */
        char * sub_str = strchr(notif, ':');

        if (sub_str != NULL) {
            int context_id = -1;
            int nw_error = strtol(sub_str + 1, NULL, 0);

            sub_str = strchr(sub_str, ',');
            if (sub_str != NULL) {
                context_id = strtol(sub_str + 1, NULL, 0);

                if (context_id >= 0 && context_id < MAX_NUM_OF_PDN_CONTEXTS) {
                    esm_error_code[context_id].esm_code = nw_error;
                }
            }
            LWM2M_INF("ESM: %d, CID: %d", nw_error, context_id);
        }

        // CNEC event parsed.
        return 0;
    }

    // Not a CNEC event.
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

    if ((mon > 1) && ((year % 4) == 0)) {
        // This year is a leap year, add the extra day.
        yday++;
    }

    /*
     * The Open Group Base Specifications Issue 7, 2018 edition
     * IEEE Std 1003.1-2017: 4.16 Seconds Since the Epoch
     *
     * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_16
     *
     * Leap year for year 2100 and later are omitted on purpose.
     */
    *p_time = sec + min * 60 + hour * 3600 + yday * 86400 +
              (year - 70) * 31536000 + ((year - 69) / 4) * 86400;

    // UTC offset as 15 min units
    *p_utc_offset = (int32_t)strtol(p_end, &p_end, 10);
}

int at_if_init(void)
{
    int err;

    // Make sure the ESM error code storage is zero
    memset((void*)esm_error_code, 0, sizeof(esm_error_code));
    restriction_error.cause = 0;
    restriction_error.validity = 0;

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

    // Register for packet domain event reporting +CGEREP.
    // The unsolicited result code is +CGEV: XXX.
    err = lwm2m_os_at_cmd_write("AT+CGEREP=1", NULL, 0);
    if (err != 0)
    {
        LWM2M_ERR("Unable to register CGEV events");
        return -1;
    }

    /* Register for EPS Session Management (ESM)
     * cause information reporting.
     * */
    err = lwm2m_os_at_cmd_write("AT+CNEC=16", NULL, 0);
    if (err != 0) {
        LWM2M_ERR("Unable to register for CNEC_ESM events");
        return -1;
    }

    // Subscribe ODIS notifications
    at_subscribe_odis();

    return 0;
}

int at_esm_error_code_get(uint8_t cid)
{
    if (cid < MAX_NUM_OF_PDN_CONTEXTS)
        return esm_error_code[cid].esm_code;
    return -1;
}

int at_esm_error_code_reset(uint8_t cid)
{
    if (cid < MAX_NUM_OF_PDN_CONTEXTS) {
        esm_error_code[cid].esm_code = 0;
        esm_error_code[cid].deactive = 0;
        return 0;
    }
    return -1;
}

int8_t at_cid_active_state(uint8_t const cid) {
    if (cid < MAX_NUM_OF_PDN_CONTEXTS)
        return esm_error_code[cid].deactive;
    return -1;
}

struct at_restriction at_restriction_error_code_get(void)
{
    return restriction_error;
}

int at_apn_register_for_packet_events(void)
{
    // Clear previous state before registering for packet domain events.
    cid_number = -1;
    cid_ipv6_link_up = IPv6_WAIT;
    restriction_error.cause = 0;
    restriction_error.validity = 0;

    return 0;
}

int at_apn_unregister_from_packet_events(void)
{
    return 0;
}

int at_read_apn_status(uint8_t *p_apn_status, uint32_t apn_status_len)
{
    int retval = lwm2m_os_at_cmd_write(AT_APN_STATUS_OP_RD, m_at_buffer,
                                       sizeof(m_at_buffer));

    if ((retval == 0) &&
        (strncmp(m_at_buffer, "%XAPNSTATUS: ", 13) == 0) &&
        ((strlen(m_at_buffer) - 13) < apn_status_len))
    {
        strncpy(p_apn_status, &m_at_buffer[13], apn_status_len);
    } else {
        retval = EIO;
    }

    return retval;
}

int at_write_apn_status(int status, const uint8_t *p_apn, uint32_t apn_len)
{
    int offset = snprintf(m_at_buffer, sizeof(m_at_buffer),
                          "%s=%d,\"", AT_APN_STATUS_OP_WR, status);

    memcpy(&m_at_buffer[offset], p_apn, apn_len);
    offset += apn_len;

    snprintf(&m_at_buffer[offset], sizeof(m_at_buffer) - offset, "\"");

    return lwm2m_os_at_cmd_write(m_at_buffer, NULL, 0);
}

int at_apn_setup_wait_for_ipv6(int *fd)
{
    int err;
    int8_t cid = -1;
    int timeout_ms = MINUTES(1);
    nrf_socklen_t len = sizeof(cid);

    if (fd == NULL) {
        return -1;
    }

    err = nrf_getsockopt(*fd, NRF_SOL_PDN, NRF_SO_PDN_CONTEXT_ID, &cid, &len);
    if (err) {
        LWM2M_ERR("Unable to get PDN context ID on socket %d, errno=%d",
                  *fd, lwm2m_os_errno());

        nrf_close(*fd);
        *fd = DEFAULT_PDN_FD;
        return -2;
    }

    LWM2M_INF("PDN cid %d found. Wait for IPv6 link...", cid);

    // Save the CID, looked up in the CGEV parser loop.
    cid_number = cid;

     // Wait until IPv6 link is up or timeout.
    while (cid_ipv6_link_up == IPv6_WAIT && timeout_ms > 0) {
        lwm2m_os_sleep(100);
        timeout_ms -= 100;
    }

    if (timeout_ms <= 0 || cid_ipv6_link_up != IPv6_LINK_UP) {
        LWM2M_ERR("Timeout/fail while waiting for IPv6 (cid=%u)", cid);
        nrf_close(*fd);
        *fd = DEFAULT_PDN_FD;
        return -3;
    }
    else {
        LWM2M_INF("IPv6 link ready for cid %d", cid);
    }

    return 0;
}

int at_read_apn_class(uint8_t apn_class, char * const p_apn, int * p_apn_len)
{
    int retval = 0;

    if ((p_apn == NULL) || (p_apn_len == NULL) || (*p_apn_len <= 0))
    {
        return EINVAL;
    }

    int written = snprintf(m_at_buffer, sizeof(m_at_buffer), "%s,%u", AT_APN_CLASS_OP_RD, apn_class);
    if ((written < 0) || (written > sizeof(m_at_buffer)))
    {
        retval = ENOMEM;
    }
    else
    {
        int err = at_response_param_to_string(m_at_buffer, 4, 2, p_apn, p_apn_len);

        if (err != 0)
        {
            // AT command error
            LWM2M_ERR("Unable to read APN Class %u. AT command error %d.", apn_class, err);
            retval = EIO;
        }
    }

    return retval;
}

int at_write_apn_class(uint8_t apn_class, const char * p_apn, int apn_len)
{
    int retval = 0;

    if (p_apn == NULL || apn_len <= 0)
    {
        return EINVAL;
    }

    static const char end_quote_and_zero[2] = {'\"', '\0'};

    int written = snprintf(m_at_buffer, sizeof(m_at_buffer), "%s,%u,\"", AT_APN_CLASS_OP_WR, apn_class);
    if ((written < 0) || (written + apn_len + sizeof(end_quote_and_zero)) >= sizeof(m_at_buffer))
    {
        return ENOMEM;
    }

    memcpy(&m_at_buffer[written], p_apn, apn_len);
    written += apn_len;
    memcpy(&m_at_buffer[written], end_quote_and_zero, sizeof(end_quote_and_zero));

    int err = lwm2m_os_at_cmd_write(m_at_buffer, m_at_buffer, sizeof(m_at_buffer));

    if (err != 0)
    {
        // AT command error
        LWM2M_ERR("Unable to write APN Class %u. AT command error %d.", apn_class, err);
        retval = EIO;
    }

    return retval;
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

int at_read_svn(char * const p_svn, int svn_len)
{
    int retval = 0;

    if (p_svn == NULL || svn_len < 3) {
        return EINVAL;
    }

    // Read IMEI.
    int len = svn_len;
    int err = at_response_param_to_string("AT+CGSN=3", 2, 1, p_svn, &len);

    if (err != 0)
    {
        // AT command error
        LWM2M_ERR("Unable to read SVN. AT command error %d.", err);
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
    int retval = 0;

    if (p_iccid == NULL || *p_iccid_len < 16) {
        return EINVAL;
    }

    // Read SIM Integrated Circuit Card Identifier (ICCID)
    int len = sizeof(m_at_buffer);
    int err = at_response_param_to_string("AT+CRSM=176,12258,0,0,10", 4, 3, m_at_buffer, &len);
    if (err == 0) {
        retval = copy_and_convert_iccid(m_at_buffer, len - 1, p_iccid, p_iccid_len);
    } else {
        LWM2M_ERR("Unable to read ICCID. AT command error.");
        retval = EIO;
    }

    return retval;
}

int at_read_firmware_version(lwm2m_string_t *p_manufacturer_id)
{
    int retval = 0;

    if (p_manufacturer_id == NULL) {
        return -EINVAL;
    }

    // Read manufacturer identification
    const char *at_cgmi = "AT+CGMR";

    retval = at_response_param_to_lwm2m_string(at_cgmi, p_manufacturer_id);

    return retval;
}

int at_read_hardware_version(lwm2m_string_t *p_hardware_version)
{
    int retval = 0;

    if (p_hardware_version == NULL) {
        return EINVAL;
    }

    // Read hardware version
    retval = lwm2m_os_at_cmd_write("AT%HWVERSION", m_at_buffer, sizeof(m_at_buffer));

    if (retval == 0 && strncmp(m_at_buffer, "%HWVERSION: ", 12) == 0)
    {
        // TODO: Use at_response_param_to_string() when this can handle strings without quotes
        char *p_end = strchr(m_at_buffer, '\r');
        (void)lwm2m_bytebuffer_to_string(&m_at_buffer[12], p_end - &m_at_buffer[12], p_hardware_version);
    }
    else
    {
        LWM2M_ERR("Unable to read AT%HWVERSION");
        retval = EIO;
    }

    return retval;
}

int at_read_operator_id(uint32_t  *p_operator_id)
{
    int retval = 0;
    struct lwm2m_os_at_param_list operid_params;

    if (p_operator_id == NULL) {
        return -EINVAL;
    }

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

void at_subscribe_odis(void)
{
    int retval = 0;

    retval = lwm2m_os_at_cmd_write("AT+ODISNTF=1", NULL, 0);

    if (retval != 0) {
        LWM2M_ERR("AT+ODISNTF=1 failed: %d", retval);
    }
}

int at_read_manufacturer(lwm2m_string_t *p_manufacturer_id)
{
    int retval = 0;

    if (p_manufacturer_id == NULL) {
        return -EINVAL;
    }

    // Read manufacturer identification
    const char *at_cgmi = "AT+CGMI";

    retval = at_response_param_to_lwm2m_string(at_cgmi, p_manufacturer_id);

    return retval;
}

int at_read_model_number(lwm2m_string_t *p_model_number)
{
    int retval = 0;

    if (p_model_number == NULL) {
        return -EINVAL;
    }

    // Read model number
    const char *at_cgmm = "AT+CGMM";

    retval = at_response_param_to_lwm2m_string(at_cgmm, p_model_number);

    return retval;
}

int at_read_radio_signal_strength_and_link_quality(int32_t * p_signal_strength, int32_t * p_link_quality)
{
    int retval = 0;
    struct lwm2m_os_at_param_list cesq_params;

    if (p_signal_strength == NULL || p_link_quality == NULL) {
        return -EINVAL;
    }

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

    if (p_cell_id == NULL) {
        return -EINVAL;
    }

    *p_cell_id = 0;

    // Read network registration status
    int len = sizeof(m_at_buffer);
    int err = at_response_param_to_string("AT+CEREG?", 6, 4, m_at_buffer, &len);

    if (err == 0)
    {
        *p_cell_id = (uint32_t)strtol(m_at_buffer, NULL, 16);
    }
    else
    {
        LWM2M_ERR("Reading cell id failed");
        retval = -EIO;
    }

    return retval;
}

int at_read_default_apn(char * p_apn, uint32_t apn_len)
{
    int retval = 0;

    if (p_apn == NULL || apn_len == 0) {
        return -EINVAL;
    }

    // Read default APN
    int len = apn_len;
    int err = at_response_param_to_string("AT+CGDCONT?", 12, 3, p_apn, &len);

    if (err != 0)
    {
        LWM2M_ERR("Unable to read default APN. AT command error %d.", err);
        retval = -EIO;
    }

    return retval;
}

int at_read_smnc_smcc(int32_t * p_smnc, int32_t *p_smcc)
{
    int retval = 0;

    if (p_smnc == NULL || p_smcc == NULL) {
        return -EINVAL;
    }

    *p_smnc = 0;
    *p_smcc = 0;

    // Read network registration status
    int len = sizeof(m_at_buffer);
    int err = at_response_param_to_string("AT+COPS?", 5, 3, m_at_buffer, &len);

    if (err == 0)
    {
        // SMNC is first 3 characters, SMNN the following characters
        *p_smcc = (int32_t)strtol(&m_at_buffer[3], NULL, 0);
        m_at_buffer[3] = '\0'; // Null termination of SMNC
        *p_smnc = (int32_t)strtol(m_at_buffer, NULL, 0);
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

    int len = sizeof(m_at_buffer);

    if (p_time == NULL || p_utc_offset == NULL || p_dst_adjustment == NULL) {
        return -EINVAL;
    }

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
            err = lwm2m_os_at_params_string_get(&cclk_params, 1, m_at_buffer, &len);
            if (err == 0)
            {
                m_at_buffer[len] = '\0';
                at_cclk_reponse_convert(m_at_buffer, p_time, p_utc_offset);
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

        lwm2m_os_at_params_list_free(&cclk_params);
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
    int retval = 0;
    int len = sizeof(m_at_buffer) - 1;

    if (p_ipaddr_list == NULL) {
        return -EINVAL;
    }

    if (p_ipaddr_list->max_len < 2)
    {
        LWM2M_ERR("IP address list too short: %d", p_ipaddr_list->max_len);
        return -ENOMEM;
    }

    // Read IP addresses
    struct lwm2m_os_at_param_list cgpaddr_params;

    int err = lwm2m_os_at_params_list_init(&cgpaddr_params, 4);
    if (err == 0)
    {
        err = at_send_command_and_parse_params("AT+CGPADDR=0", &cgpaddr_params);

        if (err == 0)
        {
            // Get IPV4 address
            err = lwm2m_os_at_params_string_get(&cgpaddr_params, 2, m_at_buffer, &len);
            if (err == 0)
            {
                m_at_buffer[len] = '\0';
                (void)lwm2m_bytebuffer_to_string(m_at_buffer, len, &p_ipaddr_list->val.p_string[0]);
                p_ipaddr_list->len = 1;
            }

            // Get IPV6 address
            len = sizeof(m_at_buffer) - 1;
            err = lwm2m_os_at_params_string_get(&cgpaddr_params, 3, m_at_buffer, &len);
            if (err == 0)
            {
                m_at_buffer[len] = '\0';
                (void)lwm2m_bytebuffer_to_string(m_at_buffer, len, &p_ipaddr_list->val.p_string[1]);
                p_ipaddr_list->len = 2;
            }
        }
        else
        {
            LWM2M_ERR("Reading IP addresses failed: %d", err);
            retval = -EIO;
        }

        lwm2m_os_at_params_list_free(&cgpaddr_params);
    }
    else
    {
        LWM2M_ERR("cgpaddr_params list init failed: %d", err);
        retval = -EINVAL;
    }

    return retval;
}

int at_read_connstat(lwm2m_connectivity_statistics_t * p_conn_stat)
{
    struct lwm2m_os_at_param_list xconnstat_params;
    int retval;

    const char *at_xconnstat = "AT%XCONNSTAT?";

    if (p_conn_stat == NULL) {
        return -EINVAL;
    }

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

int at_read_sinr_and_srxlev(int32_t * p_sinr, int32_t * p_srxlev)
{
    int retval = 0;
    struct lwm2m_os_at_param_list xsnrsq_params;

    if (p_sinr == NULL) {
        return -EINVAL;
    }

    // Read network registration status
    const char *at_xsnrsq = "AT%XSNRSQ?";

    *p_sinr = 0;

    if (lwm2m_os_at_params_list_init(&xsnrsq_params, 4) == 0)
    {
        retval = at_send_command_and_parse_params(at_xsnrsq, &xsnrsq_params);

        if (xsnrsq_params.params && retval == 0)
        {
            uint32_t sinr;
            if (lwm2m_os_at_params_int_get(&xsnrsq_params, 1, &sinr) == 0)
            {
                if (sinr <= 50)
                {
                    /*
                     *  Reported value   Measured quantity value   Unit
                     *   SS-SINR_0               SS-SINR < -24      dB
                     *   SS-SINR_1        -24 <= SS-SINR < -23      dB
                     *   SS-SINR_2        -22 <= SS-SINR < -21      dB
                     *   ...
                     *   ...
                     *   ...
                     *   SS-SINR_47        22 <= SS-SINR < 23       dB
                     *   SS-SINR_48        23 <= SS-SINR < 24       dB
                     *   SS-SINR_49        24 <= SS-SINR            dB
                     */
                    *p_sinr = -24 + (int32_t)sinr;
                }
                else
                {
                    LWM2M_WRN("SINR invalid or unknown");
                }
            }
            else
            {
                LWM2M_ERR("signal to noise ratio parse failed");
                retval = -EINVAL;
            }

            uint32_t srxlev;
            if (lwm2m_os_at_params_int_get(&xsnrsq_params, 2, &srxlev) == 0)
            {
                if (srxlev <= 255)
                {
                    /*
                     *  Reported value   Measured quantity value   Unit
                     *        0                SRXLEV <= -127       dB
                     *        1         -127 < SRXLEV <= -126       dB
                     *        2         -126 < SRXLEV <= -125       dB
                     *       ...
                     *       ...
                     *       ...
                     *       253        125 <= SRXLEV < 126         dB
                     *       254        126 <= SRXLEV < 127         dB
                     *       255        127 <= SRXLEV               dB
                     */
                    *p_srxlev = -127 + (int32_t)srxlev;
                }
                else
                {
                    LWM2M_WRN("SRXLEV invalid or unknown");
                }
            }
            else
            {
                LWM2M_ERR("cell selection RX value parse failed");
                retval = -EINVAL;
            }
        }
        else
        {
            LWM2M_ERR("reading xsnrsq failed");
            retval = -EIO;
        }

        lwm2m_os_at_params_list_free(&xsnrsq_params);
    }
    else
    {
        LWM2M_ERR("xsnrsq_params init failed");
        retval = EINVAL;
    }

    return retval;
}

int at_read_imsi(lwm2m_string_t *p_imsi)
{
    int retval = 0;

    if (p_imsi == NULL) {
        return -EINVAL;
    }

    // Read model number
    const char *at_cimi = "AT+CIMI";

    retval = at_response_param_to_lwm2m_string(at_cimi, p_imsi);

    return retval;
}

int at_read_host_device_info(lwm2m_list_t *p_list)
{
    int retval = 0;
    struct lwm2m_os_at_param_list odis_params;

    if (!p_list || !p_list->val.p_string) {
        return -EINVAL;
    }

    // Read network registration status
    const char *at_odis = "AT%ODIS?";

    retval = lwm2m_os_at_params_list_init(&odis_params, 5);
    if (retval != 0)
    {
        LWM2M_ERR("at_params_list_init failed");
        return -retval;
    }

    retval = at_send_command_and_parse_params(at_odis, &odis_params);
    if (retval != 0)
    {
        LWM2M_ERR("reading odis failed");
        return -EIO;
    }

    for (int i = 1; i < odis_params.param_count; i++)
    {
        int len = sizeof(m_at_buffer);
        retval = lwm2m_os_at_params_string_get(&odis_params, i, m_at_buffer, &len);
        if (retval != 0)
        {
            LWM2M_ERR("parse failed: no string param found");
            lwm2m_os_at_params_list_free(&odis_params);
            return -retval;
        }

        retval = lwm2m_list_string_set(p_list, i - 1, m_at_buffer, len);
        if (retval != 0)
        {
            LWM2M_ERR("failed to update host device information: invalid list definition");
            lwm2m_os_at_params_list_free(&odis_params);
            return -retval;
        }
    }

    lwm2m_os_at_params_list_free(&odis_params);

    return 0;
}

int at_write_host_device_info(lwm2m_list_t *p_list)
{
    int len;
    lwm2m_string_t *p_identity;

    if (!p_list || !p_list->val.p_string || p_list->type != LWM2M_LIST_TYPE_STRING)
    {
        return -EINVAL;
    }

    len = snprintf(m_at_buffer, sizeof(m_at_buffer), "AT+ODIS=");

    for (int i = 0; i < LWM2M_PORTFOLIO_IDENTITY_INSTANCES; i++)
    {
        p_identity = lwm2m_list_string_get(p_list, i);

        if (!p_identity)
        {
            return -EINVAL;
        }

        // The string parameters appear between double quotes and are separated by a comma.
        if (len + p_identity->len + 3 >= sizeof(m_at_buffer))
        {
            return -E2BIG;
        }

        m_at_buffer[len++] = '"';

        memcpy(&m_at_buffer[len], p_identity->p_val, p_identity->len);
        len += p_identity->len;

        m_at_buffer[len++] = '"';
        m_at_buffer[len++] = ',';
    }

    m_at_buffer[len] = '\0';

    if (lwm2m_os_at_cmd_write(m_at_buffer, NULL, 0) != 0)
    {
        return -EIO;
    }

    return 0;
}

int at_bootstrap_psk_generate(int sec_tag)
{
    int retval = 0;

    int len = snprintf(m_at_buffer, sizeof(m_at_buffer), "AT%%BSKGEN=%d,3,0", sec_tag);

    if ((len < 0) || (len > sizeof(m_at_buffer)))
    {
        retval = -ENOMEM;
    }
    else
    {
        retval = lwm2m_os_at_cmd_write(m_at_buffer, NULL, 0);

        if (retval != 0)
        {
            LWM2M_ERR("Generating bootstrap PSK failed: %d", retval);
            retval = -EIO;
        }
    }

    return retval;
}