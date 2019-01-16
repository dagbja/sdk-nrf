/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <at_params.h>

#include <kernel.h>
#include <sdk_macros.h>
#include <nrf_error.h>

#define AT_PARAMS_CALLOC    k_calloc
#define AT_PARAMS_MALLOC    k_malloc
#define AT_PARAMS_FREE      k_free

typedef enum
{
    AT_PARAM_TYPE_EMPTY = 0,   // Empty parameter. Value is ignored. Should be 0 (default).
    AT_PARAM_TYPE_NUM_SHORT,   // Numeric value stored as unsigned value on 2 bytes.
    AT_PARAM_TYPE_NUM_INT,     // Numeric value stored as unsigned value on 4 bytes.
    AT_PARAM_TYPE_STRING       // Value is a NULL terminated String. String value cannot be NULL.
} at_param_type_t;

typedef union
{
    uint16_t short_val;     // Short number (2 bytes). Stored as an unsigned number.
    uint32_t int_val;       // Integer number (4 bytes). Stored as an unsigned number.
    char * str_val;         // Pointer to a null-terminated String. Cannot be null.
} at_param_value_t;

typedef struct
{
    at_param_type_t type;   // Parameter type. Indicates how the value is encoded.
    at_param_value_t value; // Parameter value. Ignored if type is @ref AT_PARAM_TYPE_EMPTY.
} at_param_t;


/* Internal function. Parameter cannot be null. */
static void at_param_init(at_param_t * const p_param)
{
    // Initialize to default. Empty parameter with '0'/null value.
    memset(p_param, 0, sizeof(at_param_t));
}


/* Internal function. Parameter cannot be null. */
static void at_param_free(at_param_t * const p_param)
{
    // Free the allocated memory if a string was stored.
    if (p_param->type == AT_PARAM_TYPE_STRING)
    {
        AT_PARAMS_FREE(p_param->value.str_val);
    }

    // Clear integer or NULL string.
    p_param->value.int_val = 0;
}


/* Internal function. Parameters cannot be null. */
static at_param_t * at_params_get(const at_param_list_t * const p_list, uint8_t index)
{
    if (index >= p_list->param_count)
    {
        return NULL; // Out of bounds. The given index is not in range.
    }

    at_param_t * param = (at_param_t *)p_list->params;
    return &param[index];
}


/* Internal function. Parameter cannot be null. */
static size_t at_param_size(const at_param_t * const p_param)
{
    if (p_param->type == AT_PARAM_TYPE_NUM_SHORT)
    {
        return sizeof(uint16_t); // Short value.
    }
    else if (p_param->type == AT_PARAM_TYPE_NUM_INT)
    {
        return sizeof(uint32_t); // Integer value.
    }
    else if (p_param->type == AT_PARAM_TYPE_STRING)
    {
        // String cannot be null and is always stored as a null terminated string.
        return strlen(p_param->value.str_val);
    }

    return 0; // Empty or unknown parameter type.
}


uint32_t at_params_list_init(at_param_list_t * const p_list, uint8_t max_params_count)
{
    VERIFY_PARAM_NOT_NULL(p_list);

    // Check if parameter list is already initialized.
    if (p_list->params != NULL)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    // Allocate space to store max number of parameters in the array.
    p_list->params = AT_PARAMS_CALLOC(max_params_count, sizeof(at_param_t));
    if (p_list->params == NULL)
    {
        return NRF_ERROR_NO_MEM;
    }

    p_list->param_count = max_params_count;

    // Initialize all parameters in the array to defaults values.
    for (uint8_t param_idx = 0; param_idx < p_list->param_count; ++param_idx)
    {
        at_param_t * params = (at_param_t *)p_list->params;
        at_param_init(&params[param_idx]);
    }

    return NRF_SUCCESS;
}


void at_params_list_clear(at_param_list_t * const p_list)
{
    if (p_list == NULL || p_list->params == NULL)
    {
        return;
    }

    // Free any allocated memory and reset parameter type/value.
    for (uint8_t param_idx = 0; param_idx < p_list->param_count; ++param_idx)
    {
        at_param_t * params = (at_param_t *)p_list->params;
        at_param_free(&params[param_idx]);
        at_param_init(&params[param_idx]);
    }
}


void at_params_list_free(at_param_list_t * const p_list)
{
    if (p_list == NULL || p_list->params == NULL)
    {
        return;
    }

    // Clear all parameters and free any allocated memory.
    at_params_list_clear(p_list);

    // Free the parameter list.
    p_list->param_count = 0;
    AT_PARAMS_FREE(p_list->params);
    p_list->params = NULL;
}


uint32_t at_params_clear(at_param_list_t * const p_list, uint8_t index)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);

    // Check if the parameter exists in the list.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Free any allocated memory and reset parameter type/value.
    at_param_free(param);
    at_param_init(param);
    return NRF_SUCCESS;
}


uint32_t at_params_put_short(const at_param_list_t * const p_list, uint8_t index, uint16_t value)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Free previous allocated memory if any and replace parameter value.
    at_param_free(param);

    param->type = AT_PARAM_TYPE_NUM_SHORT;
    param->value.int_val = (value & USHRT_MAX); // Clear any previous value.
    return NRF_SUCCESS;
}


uint32_t at_params_put_int(const at_param_list_t * const p_list, uint8_t index, uint32_t value)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Free previous allocated memory if any and replace parameter value.
    at_param_free(param);

    param->type = AT_PARAM_TYPE_NUM_INT;
    param->value.int_val = value;
    return NRF_SUCCESS;
}


uint32_t at_params_put_string(const at_param_list_t * const p_list, uint8_t index, const char * const p_str, size_t str_len)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);
    VERIFY_PARAM_NOT_NULL(p_str);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Allocate memory to store the parameter value as a string.
    char * param_value = (char *)AT_PARAMS_MALLOC(str_len + 1);
    if (param_value == NULL)
    {
        return NRF_ERROR_NO_MEM;
    }

    // Store the string value as a null terminated string.
    memcpy(param_value, p_str, str_len);
    param_value[str_len] = '\0';

    // Free previous allocated memory if any and replace the parameter value.
    at_param_free(param);
    param->type = AT_PARAM_TYPE_STRING;
    param->value.str_val = param_value; // Never null and always null terminated.
    return NRF_SUCCESS;
}


uint32_t at_params_get_size(const at_param_list_t * const p_list, uint8_t index, size_t * const p_len)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);
    VERIFY_PARAM_NOT_NULL(p_len);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Parameter exists. Get its size.
    *p_len = at_param_size(param);
    return NRF_SUCCESS;
}


uint32_t at_params_get_short(const at_param_list_t * const p_list, uint8_t index, uint16_t * const p_value)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);
    VERIFY_PARAM_NOT_NULL(p_value);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Check if the parameter type matches the requested type.
    if (param->type != AT_PARAM_TYPE_NUM_SHORT)
    {
        return AT_PARAM_ERROR_TYPE_MISMTACH;
    }

    *p_value = param->value.short_val;
    return NRF_SUCCESS;
}


uint32_t at_params_get_int(const at_param_list_t * const p_list, uint8_t index, uint32_t * const p_value)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);
    VERIFY_PARAM_NOT_NULL(p_value);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Check if the parameter type matches the requested type.
    if (param->type != AT_PARAM_TYPE_NUM_INT)
    {
        return AT_PARAM_ERROR_TYPE_MISMTACH;
    }

    *p_value = param->value.int_val;
    return NRF_SUCCESS;
}


uint32_t at_params_get_string(const at_param_list_t * const p_list, uint8_t index, char * const p_value, size_t len)
{
    VERIFY_PARAM_NOT_NULL(p_list);
    VERIFY_PARAM_NOT_NULL(p_list->params);
    VERIFY_PARAM_NOT_NULL(p_value);

    // Check if the parameter exists.
    at_param_t * param = at_params_get(p_list, index);
    if (param == NULL)
    {
        return AT_PARAM_ERROR_INVALID_INDEX;
    }

    // Check if the parameter type matches the requested type.
    if (param->type != AT_PARAM_TYPE_STRING)
    {
        return AT_PARAM_ERROR_TYPE_MISMTACH;
    }

    // Check if the string value fits in the provided buffer.
    size_t param_len = at_param_size(param);
    if (len < param_len)
    {
        return NRF_ERROR_NO_MEM;
    }

    // Copy the string value. Not null terminated.
    memcpy(p_value, param->value.str_val, param_len);
    return NRF_SUCCESS;
}


uint32_t at_params_get_valid_count(const at_param_list_t * const p_list)
{
    if (p_list == NULL || p_list->params == NULL)
    {
        return 0; // List not allocated.
    }

    uint8_t valid_param_idx = 0;
    at_param_t * param = at_params_get(p_list, valid_param_idx);

    // Count the number of parameters until an empty parameter type is found.
    while(param != NULL && param->type != AT_PARAM_TYPE_EMPTY)
    {
        valid_param_idx += 1;
        param = at_params_get(p_list, valid_param_idx);
    }

    return valid_param_idx;
}
