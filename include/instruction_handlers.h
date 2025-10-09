
#ifndef INSTRUCTION_HANDLERS_H
#define INSTRUCTION_HANDLERS_H

#include "isa_defs.h"

static inline sword_t sign_extend_12(int val)
{
    if (val & 0x0800)
        return (sword_t)(val | 0xF000);
    return (sword_t)val;
}

void execute_alu(int func_code);
void execute_stack_op(int func_code);
void execute_branch(int func_code);

#endif
