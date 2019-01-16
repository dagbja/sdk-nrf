/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
/**@file at_cmd_parser.h
 *
 * @brief Basic parser for AT commands.
 * @{
 */
#ifndef AT_CMD_PARSER_H__
#define AT_CMD_PARSER_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <at_params.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief AT error codes. */
typedef enum
{
    AT_SUCCESS = 0x00,
    AT_CAUSE_SYNTAX_ERROR,
} at_cause_t;


/**
 * @brief Parse AT command or response parameters from a String. Save parameters in a list.
 *
 * If an error is returned by the parser, the content of @ref p_list should be ignored.
 * The @ref p_list list can be reused to parse multiple at commands. When calling this function, the list is cleared.
 * The @ref p_list list should be initialized. The size of the list defines the maximum number of parameters that can be parsed and stored.
 * If they are more parameters than @ref max_params_count, they will be ignored.
 *
 * @param p_at_params_str   AT parameters as a null-terminated String. Can be numeric or string parameters.
 * @param p_list            Pointer to an initialized list where parameters will be stored. Should not be null.
 * @param max_params_count  Maximum number of parameter expected in @ref p_at_params_str. Can be set to a smaller value to parse only some paramaters.
 *
 * @return An error code or success if parsing is successful.
 */
at_cause_t at_parser_max_params_from_str(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t max_params_count);


/**
 * @brief Parse AT command or response parameters from a String. Save parameters in a list.
 * The size of the @ref p_list list defines the number of AT parameters than can be parsed and stored.
 *
 * @see at_cause_t at_parser_max_params_from_str(const char * const p_at_params_str, at_param_list_t * const p_list, uint8_t max_params_count)
 */
at_cause_t at_parser_params_from_str(const char * const p_at_params_str, at_param_list_t * const p_list);


#ifdef __cplusplus
}
#endif

#endif // AT_CMD_PARSER_H__

/**@} */
