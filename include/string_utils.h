
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "isa_defs.h"

// To manage strings and length easily
// with our helper functions.
typedef struct
{
    word_t count;
    char   *str;
} string_t;

word_t str_pack(const char *str, word_t start_address);
string_t str_unpack(word_t buf_offset);
void buf_pack(const char *buffer, word_t buf_offset, word_t count);

#endif
