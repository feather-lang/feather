#ifndef FEATHER_CHARCLASS_H
#define FEATHER_CHARCLASS_H

#include <stddef.h>

int feather_is_octal_digit(char c);
int feather_is_digit(char c);
int feather_char_tolower(int c);
int feather_is_args_param(const char *s, size_t len);

#endif
