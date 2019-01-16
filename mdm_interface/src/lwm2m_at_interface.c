/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include <lwm2m_mdm_interface.h>

#include <zephyr.h>
#include <net/socket.h>
#include <nrf_error.h>
#include <sdk_macros.h>

#include <at_params.h>
#include <at_cmd_parser.h>

#define LWM2M_AT_IFACE_CMD_SIZE(X)         (sizeof(X) - 1)
#define LWM2M_AT_IFACE_MAX_AT_READ_LENGTH  1024
#define LWM2M_AT_IFACE_MAX_AT_PARAMS_RSP   8

#define LWM2M_AT_IFACE_VERIFY_TRUE(statement) VERIFY_TRUE(statement, NRF_ERROR_INTERNAL)

#define LWM2M_AT_IFACE_VERIFY_MODULE_INITIALIZED() \
        LWM2M_AT_IFACE_VERIFY_TRUE(m_at_socket_handle > -1)

/**
 * @brief Read data from the Modem using AT commands.
 * Handle of the AT socket. A single AT socket is used to perform all AT commands.
 */
static int m_at_socket_handle = 0;

/**
 * @brief Common buffer used to read AT responses.
 * Should be big enough to store the max AT response length.
 */
static char m_at_read_buf[LWM2M_AT_IFACE_MAX_AT_READ_LENGTH];

/**
 * @brief Shared parameter list used to store responses from all AT commands.
 */
static at_param_list_t m_param_list;


typedef struct
{
    /**
     * @brief Name of the AT command received as a response.
     */
    const char * cmd_str;

    /**
     * @brief Function used to decode the AT command response parameters.
     *
     * @param[in] p_at_params   Parameters of the AT command as a String. Cannot be null.
     * @param[out] p_out        Pointer to the output structure to populate. Can be null if not used.
     */
    uint32_t (*at_cmd_decode_handler)(const char * const p_at_params, void * const p_out);
} at_cmd_cb_t;


// Forward declarations.
static uint32_t at_CGMR_decode(const char * const p_at_params, void * const out);

static const at_cmd_cb_t m_at_cmds[] =
{
    {"+CESQ", at_CGMR_decode} ///< Extended signal quality +CESQ
};


uint32_t lwm2m_mdm_interface_init()
{
    // Initialize the shared AT parameter list used by the parser.
    uint32_t ret = at_params_list_init(&m_param_list, LWM2M_AT_IFACE_MAX_AT_PARAMS_RSP);

    if(ret != NRF_SUCCESS)
    {
        return ret;
    }

    m_at_socket_handle = socket(AF_LTE, 0, NPROTO_AT);

    if (m_at_socket_handle < 0)
    {
        at_params_list_free(&m_param_list);
        return NRF_ERROR_INTERNAL;
    }

    return NRF_SUCCESS;
}


void lwm2m_mdm_interface_uninit()
{
    at_params_list_free(&m_param_list);

    if (m_at_socket_handle >= 0)
    {
        // Gracefully close the AT socket.
        (void)close(m_at_socket_handle);
    }
}


/* Internal function. Parameters cannot be null. */
static const at_cmd_cb_t * get_at_cmd_decode_handler(const char * const p_atstring, size_t * const at_cmd_len)
{
    const at_cmd_cb_t * cb;

    for (uint8_t i = 0; i < ARRAY_SIZE(m_at_cmds); ++i)
    {
        cb = &(m_at_cmds[i]);
        size_t len = strlen(cb->cmd_str);
        if (!strncasecmp(cb->cmd_str, p_atstring, len))
        {
            *at_cmd_len = len;
            return cb;
        }
    }

    return NULL;
}


static uint32_t at_read_response(void * const p_out)
{
    // Read the response from AT socket and check the command response type.
    ssize_t len = recv(m_at_socket_handle, m_at_read_buf, LWM2M_AT_IFACE_MAX_AT_READ_LENGTH, 0);

    // We should at least get an "OK\r\n\0" back.
    LWM2M_AT_IFACE_VERIFY_TRUE(len >= 5);

    // TODO: Here we could check if we got an OK back, before decoding any command.
    // For now, the arguments from the parser with not match with the expected response,
    // but we could do more checks before invoking the parser.
    // Example result is +CESQ: 99,99,255,255,255,255\r\nOK\r\n
    // Use at_get_cmd_length and remove any space? Even if response from the modem?

    // Find the decode command handler for the received AT response.
    size_t at_cmd_len;
    char * at_cmd = (char *)m_at_read_buf;
    const at_cmd_cb_t * cb = get_at_cmd_decode_handler(at_cmd, &at_cmd_len);
    LWM2M_AT_IFACE_VERIFY_TRUE(cb);

    // Move cursor to the start of parameters list.
    at_cmd += at_cmd_len + 1;

    // Call the decode function.
    uint32_t ret = cb->at_cmd_decode_handler(at_cmd, p_out);
    return ret;
}


uint32_t lwm2m_mdm_interface_read_cesq(lwm2m_model_cesq_rsp_t * const p_cesq_rsp)
{
    LWM2M_AT_IFACE_VERIFY_MODULE_INITIALIZED();

    if (p_cesq_rsp == NULL)
    {
        // Response data model structure is required.
        return NRF_ERROR_INVALID_PARAM;
    }

    // Write the AT command and check the number of written bytes.
    static const char at_cmd[] = "AT+CESQ";
    ssize_t len = send(m_at_socket_handle, at_cmd, LWM2M_AT_IFACE_CMD_SIZE(at_cmd), 0);
    LWM2M_AT_IFACE_VERIFY_TRUE(len == LWM2M_AT_IFACE_CMD_SIZE(at_cmd));

    // Read and parse the AT command response parameters.
    uint32_t ret = at_read_response(p_cesq_rsp);

    // Clear any allocated memory used to store the parameters in the list.
    // The parameters are stored in the data model structure.
    at_params_list_clear(&m_param_list);

    return ret;
}


static uint32_t at_CGMR_decode(const char * const p_at_params, void * const out)
{
    // Parse response parameters. Expect max 6.
    uint32_t ret = at_parser_max_params_from_str(p_at_params, &m_param_list, 6);
    LWM2M_AT_IFACE_VERIFY_TRUE(ret == NRF_SUCCESS);

    // Check if the response is valid from the parser output.
    uint32_t nbr_valid_params = at_params_get_valid_count(&m_param_list);
    LWM2M_AT_IFACE_VERIFY_TRUE(nbr_valid_params == 6);

    lwm2m_model_cesq_rsp_t model;
    uint16_t value;
    ret = NRF_SUCCESS;

    ret |= at_params_get_short(&m_param_list, 0, &value);
    model.rxlev = (value & 0xFF);
    ret |= at_params_get_short(&m_param_list, 1, &value);
    model.ber = (value & 0xFF);
    ret |= at_params_get_short(&m_param_list, 2, &value);
    model.rscp = (value & 0xFF);
    ret |= at_params_get_short(&m_param_list, 3, &value);
    model.ecno = (value & 0xFF);
    ret |= at_params_get_short(&m_param_list, 4, &value);
    model.rsrq = (value & 0xFF);
    ret |= at_params_get_short(&m_param_list, 5, &value);
    model.rsrp = (value & 0xFF);

    // TODO: Do any arithmetic on the values if needed.

    if (ret == NRF_SUCCESS)
    {
        memcpy(out, &model, sizeof(lwm2m_model_cesq_rsp_t));
    }

    return ret;
}
