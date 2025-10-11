
#include "instruction_handlers.h"



// ALU execution logic (let's cry).
void execute_alu(int func_code)
{
    register sword_t s_tos = (sword_t)MEMORY[REGS.SP];
    register sword_t s_nos = (sword_t)MEMORY[REGS.SP + 1];
    register sword_t s_result;
    // This is actually deadcode.
    // register word_t  w_result;

    switch (__builtin_expect(func_code, FUNC_ADD))
    {
        // **Arithmetic (binary)**
        case FUNC_ADD:
            CHECK_SP_UNDERFLOW(2);
            s_result = s_nos + s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            ++REGS.SP;
            break;

        case FUNC_SUB:
            CHECK_SP_UNDERFLOW(2);
            s_result = s_nos - s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            ++REGS.SP;
            break;

        case FUNC_MULT:
            CHECK_SP_UNDERFLOW(2);
            s_result = s_nos * s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            ++REGS.SP;
            break;

        case FUNC_DIV:
            CHECK_SP_UNDERFLOW(2);
            if (__builtin_expect(s_tos == 0, 0)) {
                fprintf(stderr, "Divide by zero.\n");
                exit(EXIT_FAILURE);
            }
            MEMORY[REGS.SP + 1] = (word_t)(s_nos % s_tos);
            MEMORY[REGS.SP]     = (word_t)(s_nos / s_tos);
            break;

        // **Arithmetic (unary)**
        case FUNC_NEG:
            CHECK_SP_UNDERFLOW(1);
            MEMORY[REGS.SP] = (word_t)(-s_tos);
            break;

        case FUNC_INC:
            CHECK_SP_UNDERFLOW(1);
            MEMORY[REGS.SP] = (word_t)(s_tos + 1);
            break;

        case FUNC_DEC:
            CHECK_SP_UNDERFLOW(1);
            MEMORY[REGS.SP] = (word_t)(s_tos - 1);
            break;

        case FUNC_ABS:
            CHECK_SP_UNDERFLOW(1);
            MEMORY[REGS.SP] = (word_t)(s_tos < 0 ? -s_tos : s_tos);
            break;

        // **Logic**
        case FUNC_NOT:
            CHECK_SP_UNDERFLOW(1);
            MEMORY[REGS.SP] = (word_t)(!s_tos);
            break;

        case FUNC_AND:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP + 1] = (word_t)(s_nos & s_tos);
            ++REGS.SP;
            break;

        case FUNC_OR:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP + 1] = (word_t)(s_nos | s_tos);
            ++REGS.SP;
            break;

        case FUNC_XOR:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP + 1] = (word_t)(s_nos ^ s_tos);
            ++REGS.SP;
            break;

        case FUNC_SHL:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP + 1] = (word_t)(s_nos << s_tos);
            ++REGS.SP;
            break;

        case FUNC_SHR:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP + 1] = (word_t)(s_nos >> s_tos);
            ++REGS.SP;
            break;

        default:
            fprintf(stderr, "Runtime Error: Unknown ALU func code 0x%X\n", func_code);
            CLOSE_LOG();
            exit(EXIT_FAILURE);
    }
}



void execute_stack_op(int func_code)
{
    register word_t w_tos = MEMORY[REGS.SP];
    register word_t w_nos = MEMORY[REGS.SP + 1];

    switch (__builtin_expect(func_code, FUNC_DUP))
    {
        case FUNC_SWAP:
            CHECK_SP_UNDERFLOW(2);
            MEMORY[REGS.SP]     = w_nos;
            MEMORY[REGS.SP + 1] = w_tos;
            break;

        case FUNC_DUP:
            CHECK_SP_UNDERFLOW(1);
            CHECK_SP_OVERFLOW(1);
            MEMORY[--REGS.SP] = w_tos;
            break;

        case FUNC_DROP:
            CHECK_SP_UNDERFLOW(1);
            ++REGS.SP;
            break;

        case FUNC_OVER:
            CHECK_SP_UNDERFLOW(2);
            CHECK_SP_OVERFLOW(1);
            MEMORY[--REGS.SP] = w_nos;
            break;

        default:
            fprintf(stderr, "Unknown stack func 0x%X\n", func_code);
            CLOSE_LOG();
            exit(EXIT_FAILURE);
    }
}



void execute_branch(int func_code)
{
    switch (__builtin_expect(func_code, FUNC_BEQ))
    {
        case FUNC_BEQ:
        {
            CHECK_SP_UNDERFLOW(3);
            word_t offset = MEMORY[REGS.SP];
            word_t rhs    = MEMORY[REGS.SP + 1];
            word_t lhs    = MEMORY[REGS.SP + 2];
            REGS.SP += 3;
            if (lhs == rhs)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        case FUNC_BNE:
        {
            CHECK_SP_UNDERFLOW(3);
            word_t offset = MEMORY[REGS.SP];
            word_t rhs    = MEMORY[REGS.SP + 1];
            word_t lhs    = MEMORY[REGS.SP + 2];
            REGS.SP += 3;
            if (lhs != rhs)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        case FUNC_BZ:
        {
            CHECK_SP_UNDERFLOW(2);
            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];
            REGS.SP += 2;
            if (val == 0)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        case FUNC_BNZ:
        {
            CHECK_SP_UNDERFLOW(2);
            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];
            REGS.SP += 2;
            if (val != 0)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        case FUNC_BN:
        {
            CHECK_SP_UNDERFLOW(2);
            word_t offset = MEMORY[REGS.SP];
            sword_t val   = (sword_t)MEMORY[REGS.SP + 1];
            REGS.SP += 2;
            if (val < 0)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        case FUNC_BP:
        {
            CHECK_SP_UNDERFLOW(2);
            word_t offset = MEMORY[REGS.SP];
            sword_t val   = (sword_t)MEMORY[REGS.SP + 1];
            REGS.SP += 2;
            if (val > 0)
                REGS.PC += sign_extend_12(offset);
            break;
        }

        default:
            fprintf(stderr, "Unknown branch func 0x%X\n", func_code);
            CLOSE_LOG();
            exit(EXIT_FAILURE);
    }
}
