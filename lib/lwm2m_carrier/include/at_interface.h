/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AT_INTERFACE_H__
#define AT_INTERFACE_H__

#include <stdbool.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>


typedef void (*at_net_reg_stat_cb_t)(uint32_t net_stat);

/* TODO: Move APIs to modem interface modem with data model. */

int at_if_init(void);

/**
 * @brief Get last ESM error_code reported
 *
 * @return ESM error code.
 */
uint32_t at_esm_error_code_get(void);

/**
 * @brief Register for packet domain events
 *
 * @return Zero on success, -1 otherwise.
 */
int at_apn_register_for_packet_events(void);

/**
 * @brief Unregister from packet domain events
 *
 * @return Zero on success, -1 otherwise.
 */
int at_apn_unregister_from_packet_events(void);

/**
 * @brief Wait for IPV6 link on APN.
 * Wait until IPv6 link is ready. Return an error after a timeout of one minute.
 *
 * @param[in,out] fd PDN socket handle.
 *
 * @return Zero on success, -1 otherwise.
 */
int at_apn_setup_wait_for_ipv6(int *fd);

/**
 * @brief Read APN class name.
 * APN Class name has max size of 63 bytes excluding null-terminator.
 *
 * @param[in]    apn_class The APN Class to lookup the APN for.
 * @param[out]   p_apn     The buffer where to store the APN.
 * @param[inout] p_apn_len Size of the buffer as input. Length of
 *                         the APN as output.
 *
 * @retval EINVAL Invalid parameters.
 * @retval EIO    AT command error.
 * @retval ENOMEM Send buffer was not large enough for the generated AT command.
 * @retval 0 Success.
 */
int at_read_apn_class(uint8_t apn_class, char * const p_apn, int * p_apn_len);

/**
 * @brief Write APN class name.
 *
 * @param[in]  apn_class   The APN Class to write the APN for.
 * @param[in]  p_apn       The buffer where the APN is stored.
 * @param[in]  apn_len     Size of the APN.
 *
 * @retval EINVAL Invalid parameters.
 * @retval EIO    AT command error.
 * @retval ENOMEM Send buffer was not large enough for the generated AT command.
 * @retval 0 Success.
 */
int at_write_apn_class(uint8_t apn_class, const char * p_apn, int apn_len);

/**
 * @brief Read device IMEI.
 * IMEI is always 14 digits and a check digit.
 *
 * @param p_imei[in] Buffer to store IMEI. Stored as a NULL-terminated String with 15 digits.
 * @param imei_len[in] Size of the buffer. Must be at least 16 bytes.
 *
 * @return Result of the IMEI read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval 0 Success.
 */
int at_read_imei(char * const p_imei, int imei_len);

/**
 * @brief Read device SVN.
 * SVN is always 2 digits.
 *
 * @param p_svn[in] Buffer to store SVN. Stored as a NULL-terminated String with 2 digits.
 * @param svn_len[in] Size of the buffer. Must be at least 3 bytes.
 *
 * @return Result of the SVN read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval 0 Success.
 */
int at_read_svn(char * const p_svn, int svn_len);

/**
 * @brief Read subscriber number (MSISDN).
 * MSISDN is maximum 15 digits. Length may varies based on the operator.
 * MSISDN may not always be available depending on the SIM card.
 *
 * @param p_msisdn[in] Buffer to store IMEI. Store a NULL-terminated String with max 15 digits MSISDN.
 * @param msisdn_len[in] Size of the buffer. Must be at least 16 bytes.
 *
 * @return Result of the MSISDN read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval EPERM No MSISDN available on the SIM card.
 * @retval 0 Success.
 */
int at_read_msisdn(char * const p_msisdn, int msisdn_len);

/**
 * @brief Read SIM Integrated Circuit Card Identifier (ICCID).
 *
 * @param p_iccid[in] Buffer to store ICCID. Store a NULL-terminated String with max 20 digits.
 * @param p_iccid_len[in] Size of the buffer. Must be at least 20 bytes.
 *
 * @return Result of the ICCID read AT command.
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval 0 Success.
 */
int at_read_sim_iccid(char *p_iccid, uint32_t * p_iccid_len);

/**
 * @brief Read the Modem firmware version name.
 *
 * @param[out] p_manufacturer_id Pointer to store the firmware version
 *
 * Version name is something like "mfw_nrf9160_0.7.0-15.alpha".
 */
int at_read_firmware_version(lwm2m_string_t *p_manufacturer_id);

/**
 * @brief Read operator ID from modem as defined in XOPERID at command.
 *
 * @param[out] p_oper_id Pointer to store the operator ID
 * @return An error code if the read failed.
 */
int at_read_operator_id(uint32_t *p_oper_id);

/**
 * @brief Read manufacturer string from modem.
 *
 * @param[out] p_manufacturer_id Pointer to store the manufacturer as lwm2m_string_t
 * @return An error code if the read failed.
 */
int at_read_manufacturer(lwm2m_string_t * p_manufacturer_id);

/**
 * @brief Read model string from modem.
 *
 * @param[out] p_model_number Pointer to store the model as lwm2m_string_t
 * @return An error code if the read failed.
 */
int at_read_model_number(lwm2m_string_t * p_model_number);

/**
 * @brief Read radio signal strength. Converted from RSRP to dBm.
 *
 * @param[out] p_signal_strength Pointer to store signal strength in dBm.
 * @return An error code if the read failed.
 */
int at_read_radio_signal_strength_and_link_quality(int32_t * p_signal_strength, int32_t * p_link_quality);

/**
 * @brief Read E-UTRAN cell ID.
 *
 * @param[out] p_model_number Pointer to store the cell ID
 * @return An error code if the read failed.
 */
int at_read_cell_id(uint32_t * p_cell_id);

/**
 * @brief Read default APN.
 *
 * @param p_apn[out]  Buffer to store APN. Store a NULL-terminated string.
 * @param apn_len[in] Size of the buffer.
 *
 * @retval EINVAL Invalid parameters.
 * @retval EIO AT command error.
 * @retval 0 Success.
 */
int at_read_default_apn(char * p_apn, uint32_t apn_len);

/**
 * @brief Read Mobile Country Code (MCC) and Mobile Network Code (MNC) values from modem.
 *
 * @param[out] p_smnc Pointer to store the MNC value
 * @param[out] p_smcc Pointer to store the MCC value
 * @return An error code if the read failed.
 */
int at_read_smnc_smcc(int32_t * p_smnc, int32_t * p_smcc);

/**
 * @brief Read time and UTC offset from modem.
 *
 * @param[out] p_time           Pointer to store the time as seconds since Epoch
 * @param[out] p_utc_offset     Pointer to store the UTC offset as units of 15 minutes
 * @param[out] p_dst_adjustment Pointer to store the daylight saving adjustment as units of hours
 * @return An error code if the read failed.
 */
int at_read_time(int32_t * p_time, int32_t * p_utc_offset, int32_t * p_dst_adjustment);

/**
 * @brief Read IP addresses from modem.
 *
 * @param[out] p_ipaddr_list       Pointer to list to store the IP addresses
 * @return An error code if the read failed.
 */
int at_read_ipaddr(lwm2m_list_t * p_ipaddr_list);

/**
 * @brief Subscribe to notifications in change of network registration status
 *
 * @param[in] net_reg_stat_cb       Callback function to be called on changes
 */
void at_subscribe_net_reg_stat(at_net_reg_stat_cb_t net_reg_stat_cb);

void at_subscribe_esm(void);

int at_read_connstat(lwm2m_connectivity_statistics_t * p_conn_stat);
int at_start_connstat(void);
int at_stop_connstat(void);

/**
 * @brief Read radio signal to noise ratio.
 *
 * @param[out] p_sinr Pointer to store signal to noise ratio.
 *
 * @return An error code if the read failed.
 */
int at_read_radio_signal_to_noise_ratio(int32_t * p_sinr);

/**
 * @brief Read IMSI.
 *
 * @param[out] p_sinr Pointer to store IMSI.
 *
 * @return An error code if the read failed.
 */
int at_read_imsi(lwm2m_string_t *p_imsi);

#endif // AT_INTERFACE_H__
