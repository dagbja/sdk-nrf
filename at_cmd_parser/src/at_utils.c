/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "at_utils.h"

uint32_t at_remove_spaces_from_begining(char **p_str)
{
    if ((!p_str) || (!(*p_str)))
    {
        return 0;
    }

    uint32_t space_count = 0;

    while (isspace(**p_str) && (**p_str))
    {
        space_count++;
        (*p_str)++;
    }
    return space_count;
}

size_t at_get_cmd_length(const char * const p_str)
{
    if (p_str == NULL)
    {
        return 0;
    }

    size_t len = 0;
    char * at = (char *)p_str;
    while (*at && (*at != '?') && (*at != ';'))
    {
        len++;
        at++;
    }
    return len;
}
