/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
/**@file at_utils.h
 *
 * @brief AT parser utility functions to deal with strings.
 * @{
 */
#ifndef AT_UTILS_H__
#define AT_UTILS_H__

#include <stdint.h>

#include "compiler_abstraction.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief at_remove_spaces_from_begining
 * Skips spaces from the beginning of the string and moves pointer to point first non-space character.
 * Caller should maintain pointer to block start for deallocation purposes.
 *
 * @param[in] p_str address of string pointer
 * @param[out] p_str p_str pointer changed to point to first non-space character.
 * @return number of removed spaces.
 */
uint32_t at_remove_spaces_from_begining(char **p_str);


/**
 * @brief at_get_cmd_length
 * Counts length of AT command terminated by '\0', '?' or AT_CMD_SEPARATOR.
 *
 * @param[in] p_str Pointer to AT command string
 * @return length of first AT command.
*/
size_t at_get_cmd_length(const char * const p_str);


#ifdef __cplusplus
}
#endif

#endif // AT_CMD_H__

/**@} */
