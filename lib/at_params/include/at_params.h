/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
/**@file at_params.h
 *
 * @brief Store a list of AT commands/responses parameters.
 *
 * A parameter list contains an array of parameters defined by a type
 * and a value. Those parameters could be arguments of an AT command for
 * instance. They can be numeric or string values.
 * The same list of parameters can be reused. Each parameter can be
 * updated or cleared. Once the parameter list is created, its size
 * cannot be changed. All parameters values are copied in the list.
 * Parameters should be cleared to free that memory. Getters and setters
 * methods are available to read parameter values.
 *
 * @{
 */
#ifndef AT_PARAMS_H__
#define AT_PARAMS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief List of AT parameters that compose an AT command or response.
 *
 * Contains an array of opaque data. Setter and getter methods should be used
 * to get access to the parameters in the array.
 */
typedef struct
{
    uint8_t param_count;   ///< Number of parameters in the array.
    void * params;         ///< Array of parameters. Contains @ref param_count elements.
} at_param_list_t;


/**
 * @brief Invalid parameter index. Index not in range, parameter not found.
 */
#define AT_PARAM_ERROR_INVALID_INDEX        0x100

/**
 * @brief When reading a parameter value, the actual parameter type does not match with the requested type.
 */
#define AT_PARAM_ERROR_TYPE_MISMTACH        0x102


/**
 * @brief Create a list of parameters.
 * An array of @param max_params_count is allocated. Each parameter are initialized to default values.
 * Should not be called again before freeing the list.
 *
 * @param[in] p_list List of parameters to initialize.
 * @param[in] max_params_count Maximum number of element that the list can store.
 *
 * @retval NRF_ERROR_NULL          Null parameter.
 * @retval NRF_ERROR_INVALID_STATE List already initialized. Must be freed first.
 * @retval NRF_ERROR_NO_MEM        No enough memory to allocate the list.
 * @retval NRF_SUCCESS             List created successfully and elements initialized.
 */
uint32_t at_params_list_init(at_param_list_t * const p_list, uint8_t max_params_count);


/**
 * @brief Clear/reset all parameter types and values.
 * All parameter types and values are reset to defaults values.
 *
 * @param[in] p_list List of parameters to clear.
 */
void at_params_list_clear(at_param_list_t * const p_list);


/**
 * @brief Free a list of parameters.
 * First the list is cleared and then the list and its elements are deleted.
 *
 * @param[in] p_list List of parameters to free.
 */
void at_params_list_free(at_param_list_t * const p_list);


/**
 * @brief Clear/reset a parameter type and value.
 *
 * @param[in] p_list List of parameters to clear.
 * @param[in] index  Parameter index to clear.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval NRF_SUCCESS                  Parameter added.
 */
uint32_t at_params_clear(at_param_list_t * const p_list, uint8_t index);


/**
 * @brief Add a parameter in the list at the specified index and assign it a short value.
 * Any previous parameter will be replaced.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Index in the list were to put the parameter.
 * @param[in] value     Parameter value.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval NRF_SUCCESS                  Parameter added.
 */
uint32_t at_params_put_short(const at_param_list_t * const p_list, uint8_t index, uint16_t value);


/**
 * @brief Add a parameter in the list at the specified index and assign it an integer value.
 * Any previous parameter will be replaced.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Index in the list were to put the parameter.
 * @param[in] value     Parameter value.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval NRF_SUCCESS                  Parameter added.
 */
uint32_t at_params_put_int(const at_param_list_t * const p_list, uint8_t index, uint32_t value);


/**
 * @brief Add a parameter in the list at the specified index and assign it a string value.
 * The parameter string value is copied and added to the list. A null terminated string is stored.
 * Any previous parameter will be replaced.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Index in the list were to put the parameter.
 * @param[in] p_str     Pointer to the string value.
 * @param[in] str_len   Number of characters of the string value @ref p_str.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval NRF_ERROR_NO_MEM             No memory to allocate the parameter value.
 * @retval NRF_SUCCESS                  Parameter added.
 */
uint32_t at_params_put_string(const at_param_list_t * const p_list, uint8_t index, const char * const p_str, size_t str_len);


/**
 * @brief Get the size of a given parameter in byte.
 * A missing parameter has a size of '0'.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Parameter index in the list.
 * @param[out] p_len    Length of the parameter in byte or the string length.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval NRF_SUCCESS                  Parameter size is valid.
 */
uint32_t at_params_get_size(const at_param_list_t * const p_list, uint8_t index, size_t * const p_len);


/**
 * @brief Get a parameter value as a short number.
 * Numeric values are stored as unsigned number. The parameter type has to be a short or an error is returned.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Parameter index in the list.
 * @param[out] p_value  Parameter value.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval AT_ERROR_PARAM_TYPE_MISMTACH Parameter type at @ref index is not a short.
 * @retval NRF_SUCCESS                  Parameter type and value are valid.
 */
uint32_t at_params_get_short(const at_param_list_t * const p_list, uint8_t index, uint16_t * const p_value);


/**
 * @brief Get a parameter value as an integer number.
 * Numeric values are stored as unsigned number. The parameter type has to be an integer or an error is returned.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Parameter index in the list.
 * @param[out] p_value  Parameter value.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval AT_ERROR_PARAM_TYPE_MISMTACH Parameter type at @ref index is not an integer.
 * @retval NRF_SUCCESS                  Parameter type and value are valid.
 */
uint32_t at_params_get_int(const at_param_list_t * const p_list, uint8_t index, uint32_t * const p_value);


/**
 * @brief Get a parameter value as a string.
 * The parameter type has to be a string or an error is returned.
 * The string parameter value is copied to the buffer.
 * @ref len should be bigger than the string length or an error is returned.
 * The copied string is not NULL terminated.
 *
 * @param[in] p_list    List of parameters.
 * @param[in] index     Parameter index in the list.
 * @param[in] p_value   Pointer to the buffer to copy the value to.
 * @param[in] len       Available space in @param p_value.
 *
 * @retval NRF_ERROR_NULL               Null parameters.
 * @retval NRF_ERROR_NO_MEM             The buffer is too small to copy the string parameter value.
 * @retval AT_ERROR_INVALID_PARAM_INDEX Invalid parameter index. Parameter not found.
 * @retval AT_ERROR_PARAM_TYPE_MISMTACH Parameter type at @ref index is not a string.
 * @retval NRF_SUCCESS                  Parameter type and value are valid.
 */
uint32_t at_params_get_string(const at_param_list_t * const p_list, uint8_t index, char * const p_value, size_t len);


/**
 * @brief Get the number of valid parameters in the list.
 *
 * @param[in] p_list    List of parameters.
 *
 * @return The number of valid parameters, until an empty parameter is found.
 */
uint32_t at_params_get_valid_count(const at_param_list_t * const p_list);


#ifdef __cplusplus
}
#endif

#endif // AT_PARAMS_H__

/**@} */
