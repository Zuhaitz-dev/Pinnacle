
// I know, there's a *bit* of inspiration with LC-3.

#ifndef ISA_DEFS_H
#define ISA_DEFS_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// So, we start with the Architectural Constants.
#define MEMORY_SIZE 65536           // 2^16 words.
#define CODE_START  0x0001          // The start address for program code.
#define INITIAL_SP MEMORY_SIZE - 1  // The stack starts at top of the memory.

// A function prototype.
void load_memory(const char *filename);

// The essential data types.
typedef uint16_t word_t;
typedef int16_t sword_t;

#ifndef NO_LOG
    extern FILE *log_file;
#   define CLOSE_LOG() fclose(log_file)
#else
#   define CLOSE_LOG() ((void)0)
#endif

// Stack macros.
#ifdef BENCHMARK
#   define CHECK_SP_OVERFLOW(n) ((void)0)
#   define CHECK_SP_UNDERFLOW(n) ((void)0)
#else
#   define CHECK_SP_UNDERFLOW(n) if (REGS.SP + n > INITIAL_SP) { fprintf(stderr, "Stack underflow\n"); CLOSE_LOG(); exit(EXIT_FAILURE); }
#   define CHECK_SP_OVERFLOW(n)  if (REGS.SP < n) { fprintf(stderr, "Stack overflow\n"); CLOSE_LOG(); exit(EXIT_FAILURE); }
#endif

// Memory helpers.
static inline char GET_CHAR_FROM_WORD(word_t w, int idx)
{
    return (idx % 2 == 0) ? ((w >> 8) & 0xFF) : (w & 0xFF);
}

static inline word_t PACK_CHARS(char hi, char lo)
{
    return ((hi & 0xFF) << 8) | (lo & 0xFF);
}

static inline sword_t sign_extend_12(int val)
{
    if (val & 0x0800)
        return (sword_t)(val | 0xF000);
    return (sword_t)val;
}

// TRAP Table Definitions.
typedef void (*trap_handler_t)(void);
extern trap_handler_t TRAP_TABLE[256];

// To manage strings and length easily
// with our helper functions.
typedef struct
{
    word_t count;
    char   *str;
} string_t;

// In Unix-like systems, like Linux, the exit code is 1 byte long.
typedef uint8_t exitcode_t;
extern exitcode_t status; // Use extern to declare, not define

// Instruction Format Union.
// Easy and clear decoding for both formats,
// as both use the pattern: Opcode (4 bits) + Argument (12 bits).
typedef union
{
    word_t raw;
    struct
    {
        uint16_t arg  : 12;   // function Code, immediate, or offset.
        uint16_t opcode : 4;
    } fields;
} Instruction;

// The Architectural Registers.
typedef struct
{
    word_t PC;  // Program Counter.
    word_t SP;  // Stack Pointer (Which points to TOS, aka Top Of Stack).
    word_t BR;  // Base Register.
    word_t LR;  // Link Register (Return address for JAL)
} Registers;

// Memory and Register Globals.
extern word_t    MEMORY[MEMORY_SIZE];
extern Registers REGS;

enum Opcodes
{
    OP_ILLEGAL    = 0x0, // Format 1: Illegal (Func ignored)
    OP_ALU_LOGIC  = 0x1, // Format 1: 0-Operand (Uses Func)
    OP_STACK_OPS  = 0x2, // Format 1: 0-Operand (Uses Func)
    OP_BRANCH     = 0x3, // Format 1: 0-Operand (Uses Func)
    OP_LDI        = 0x4, // Format 2: Load Immediate
    OP_LOAD       = 0x5, // Format 2: Load (Base+Offset)
    OP_STORE      = 0x6, // Format 2: Store (Base+Offset)
    OP_JMP        = 0x7, // Format 2: Jump Unconditional
    OP_JAL        = 0x8, // Format 2: Jump and Link
    OP_RET        = 0x9, // Format 1: Ret (Func ignored)
    OP_MACRO      = 0xA, // Format 2: Define macro.
    OP_ENDMACRO   = 0xB, // Format 2: end of definition.
    OP_TRAP       = 0xE, // Format 2: Call host system service
    OP_HALT       = 0xF  // Format 1: Halt (Func ignored)
};

#define ALU_LOGIC_OP_OFFSET 0x800

enum AluFuncs
{
    // ARITHMETIC.
    FUNC_ADD  = 0x000,
    FUNC_SUB  = 0x001,
    FUNC_MULT = 0x002,
    FUNC_DIV  = 0x003,
    FUNC_NEG  = 0x004,
    FUNC_INC  = 0x005,
    FUNC_DEC  = 0x006,
    FUNC_ABS  = 0x007,
    // LOGIC
    FUNC_NOT  = 0x800,
    FUNC_AND  = 0x801,
    FUNC_OR   = 0x802,
    FUNC_XOR  = 0x803,
    // SHIFT
    FUNC_SHL  = 0x804,
    FUNC_SHR  = 0x805
};

enum StackOpsFuncs
{
    FUNC_SWAP = 0x000,
    FUNC_DUP  = 0x001,
    FUNC_DROP = 0x002,
    FUNC_OVER = 0x003
};

enum BranchFuncs
{
    FUNC_BEQ = 0x000,
    FUNC_BNE = 0x001,
    FUNC_BZ  = 0x002,
    FUNC_BNZ = 0x003,
    FUNC_BN  = 0x004,
    FUNC_BP  = 0x005
};

#endif
