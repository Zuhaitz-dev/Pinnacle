
#ifndef TRAP_HANDLERS_H
#define TRAP_HANDLERS_H

#include "isa_defs.h"

// Could be changed later, but sounds reasonable right now.
#define MAX_STACK_READ_SIZE 4096

void initialize_trap_table(void);
string_t unpack_string(word_t buf_offset, word_t count);

#endif
