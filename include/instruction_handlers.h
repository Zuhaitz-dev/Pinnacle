
#ifndef INSTRUCTION_HANDLERS_H
#define INSTRUCTION_HANDLERS_H

#include "isa_defs.h"

void execute_alu(int func_code);
void execute_stack_op(int func_code);
void execute_branch(int func_code);

#endif
