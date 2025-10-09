
#ifndef TRAP_HANDLERS_H
#define TRAP_HANDLERS_H

#include "isa_defs.h"

void initialize_trap_table(void);
string_t unpack_string(word_t buf_offset, word_t count);

#endif
