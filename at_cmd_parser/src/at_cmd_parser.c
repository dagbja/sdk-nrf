/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>

#include "at_cmd_parser.h"

#include <at_params.h>
#include <at_utils.h>

#include "nrf_error.h"
#include "sdk_macros.h"

/**
 * @brief The parsed parameter is not a numeric value.
 * Internal parsing error code
 */
#define AT_CMD_PARSER_ERROR_PARAM_NOT_NUMERIC      0x103

#define AT_CMD_PARAM_SEPARATOR      ','
#define AT_CMD_SEPARATOR            ';'

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Internal function. Parameters cannot be null. String must be null terminated. */
static uint32_t at_parse_param_uint32(char * p_at_params_str, uint32_t * p_val, size_t * p_consumed)
{
    uint32_t value = 0;
    uint32_t value1;
    bool negative = false;
    char * at_str = (char *)p_at_params_str;

    *p_consumed = 0;

    // Check if this is a negative number.
    if(*at_str == '-' && isdigit(*(at_str + 1)))
    {
        at_str++;
        (*p_consumed)++;
        negative = true;
    }

    // Check if this is a positive number.
    else if (!isdigit(*at_str))
    {
        return AT_CMD_PARSER_ERROR_PARAM_NOT_NUMERIC;
    }

    while (isdigit(*at_str))
    {
        if (value > 0xFFFFFFFE)
        {
            return AT_CMD_PARSER_ERROR_PARAM_NOT_NUMERIC;
        }
        value1 = value * 10;
        value  = value1 + ( (uint8_t)(*at_str) - '0');

        if ((value < value1) || (value == 0xFFFFFFFF))
        {
            return AT_CMD_PARSER_ERROR_PARAM_NOT_NUMERIC;
        }
        at_str++;
        (*p_consumed)++;
    }

    if (negative == true)
    {
        value = (uint32_t)(0 - value);
    }

    *p_val = value;
    return NRF_SUCCESS;
}


/* Internal function. Parameters cannot be null. String must be null terminated. */
static uint32_t at_parse_param_numeric(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t index, size_t * p_consumed)
{
    // Remove any spaces before parsing any numbers.
    char * at_str = (char *)p_at_params_str;
    size_t num_spaces = at_remove_spaces_from_begining(&at_str);

    uint32_t val; // Parsed value, max size.
    size_t consumed_bytes;
    uint32_t ret = at_parse_param_uint32(at_str, &val, &consumed_bytes);
    if (ret != NRF_SUCCESS)
    {
        *p_consumed = 0;
        return AT_CMD_PARSER_ERROR_PARAM_NOT_NUMERIC;
    }

    if (val <= USHRT_MAX)
    {
        ret = at_params_put_short(p_list, index, (uint16_t)(val));
    }
    else
    {
        ret = at_params_put_int(p_list, index, val);
    }

    // Update the number of bytes consumed. Do not update the original string.
    *p_consumed = consumed_bytes + num_spaces;
    return NRF_SUCCESS;
}


/* Internal function. Parameters cannot be null. String must be null terminated. */
static uint32_t at_parse_param_string(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t index, size_t * p_consumed)
{
    uint16_t str_len;
    bool in_double_quotes = false;

    char * str = (char *)p_at_params_str;
    if (str == NULL || *str == '\0')
    {
        return NRF_SUCCESS;
    }

    // Remove spaces. String parameters with spaces should be inside double quotes.
    size_t spaces = at_remove_spaces_from_begining(&str);

    if (*str == '\"')
    {
        // Start of string parameter value inside double quotes.
        str++;
        in_double_quotes = true;
    }

    // FIXME: the following code can be used to parse IP addresses if needed.

//    uint8_t no_of_dots = 0;
//    uint8_t no_of_colon = 0;
//    bool is_ip = true;

    // Start of the actual parameter value, after any spaces or double quotes.
    char * param_value_start = str;

    // Move cursor until end of the parameter string value.
    while ((*str != '\0') &&
           ((!in_double_quotes &&
           ((*str != AT_CMD_SEPARATOR)||(*str != AT_CMD_PARAM_SEPARATOR))) ||
           (in_double_quotes && (*str != '\"'))))
    {
//        if (*str == '.')
//        {
//            no_of_dots++;
//        }
//        if (*str == ':')
//        {
//            no_of_colon++;
//        }
//        if (is_ip)
//        {
//            if (!(isdigit(*str) || isxdigit(*str) || (*str == '.') || (*str == ':')))
//            {
//                is_ip = false;
//            }
//        }
        str++;
    }

    if (in_double_quotes && ((*str == AT_CMD_SEPARATOR) || (*str == '\0')))
    {
        return AT_CAUSE_SYNTAX_ERROR;
    }

    str_len = str - param_value_start;
    *p_consumed = str_len + spaces;

    if (in_double_quotes && (*str == '\"'))
    {
        // End of String parameter inside double quotes. Double quotes are not saved.
        str++;
        *p_consumed += 2;
        return at_params_put_string(p_list, index, param_value_start, str_len);
    }

    // Add parameter value as a String.
    return at_params_put_string(p_list, index, param_value_start, str_len);

//    if (in_double_quotes)
//    {
//        //TODO: IPV4 should have 3 dots and IPV6 has 2-7 colons. This does not detect IP strings.
//        if (is_ip && (no_of_dots == AT_NO_OF_BYTES_IN_IPV4))
//        {
//            p_param->type = AT_PARAM_TYPE_STRING_IPV4;
//        }
//        else if (is_ip && (no_of_colon == AT_NO_OF_BYTES_IN_IPV6))
//        {
//            p_param->type = AT_PARAM_TYPE_STRING_IPV6;
//        }
//        else
//        {
//            p_param->type = AT_PARAM_TYPE_STRING;
//        }
//
//        p_param->type = AT_PARAM_TYPE_STRING;
//    }
//    else
//    {
//        p_param->type = AT_PARAM_TYPE_UNKNOWN_STRING;
//    }
}


/* Internal function. Parameters cannot be null. String must be null terminated. */
static uint32_t at_parse_param(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t index, size_t * p_consumed)
{
    // Parse the at string to get the first parameter.
    if (p_at_params_str == NULL || *p_at_params_str == '\0' ||
        *p_at_params_str == AT_CMD_PARAM_SEPARATOR || *p_at_params_str == AT_CMD_SEPARATOR)
    {
        // End of the command. No more parameter to parse.
        // Expected number of parameters are less than the maximum number of parameters.
        // Clear them in case the parameter list is reused.
        *p_consumed = 0;
        (void)at_params_clear(p_list, index);
        return NRF_SUCCESS;
    }

    // First try to parse the parameter value as a number.
    uint32_t ret = at_parse_param_numeric(p_at_params_str, p_list, index, p_consumed);

    if (ret != NRF_SUCCESS)
    {
        // Try to parse the parameter value as a string.
        ret = at_parse_param_string(p_at_params_str, p_list, index, p_consumed);
    }

    return ret;
}


at_cause_t at_parser_params_from_str(const char * const p_at_params_str, at_param_list_t * const p_list)
{
    // Parse the maximum  number of parameters that we can store in the list.
    return at_parser_max_params_from_str(p_at_params_str, p_list, p_list->param_count);
}


at_cause_t at_parser_max_params_from_str(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t max_params_count)
{
    VERIFY_PARAM_NOT_NULL(p_at_params_str);
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);

    // Remove all previous parameters if any.
    at_params_list_clear(p_list);

    // Maximum allowed of parameters to store.
    max_params_count = MIN(max_params_count, p_list->param_count);

    // Remove spaces between parameters.
    char * str = (char *)p_at_params_str;
    (void)at_remove_spaces_from_begining(&str);

    // Start parsing the maximum expected of parameters.
    for (uint8_t param_idx = 0; param_idx < max_params_count; ++param_idx)
    {
        size_t consumed;
        uint32_t ret = at_parse_param(str, p_list, param_idx, &consumed);

        if(ret != NRF_SUCCESS)
        {
            // Error when parsing one parameter.
            return ret;
        }

        // Parameter added. Continue parsing after this parameter.
        str += consumed;

        if (param_idx < (max_params_count - 1) && *str != '\0')
        {
            if (*str == AT_CMD_PARAM_SEPARATOR)
            {
                str++; // Moving cursor to next parameter value.
            }
            else if((*str == '\r') || (*str == '\n'))
            {
                return NRF_SUCCESS;
            }
            else
            {
                return AT_CAUSE_SYNTAX_ERROR;
            }
        }
    }

    return NRF_SUCCESS;
}
