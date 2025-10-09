
#define _POSIX_C_SOURCE 199309L

#include "isa_defs.h"
#include "instruction_handlers.h"
#include "trap_handlers.h"
#include <stdlib.h>
#ifdef BENCHMARK
#   include <time.h>
#endif



// Global definition of memory and registers.
word_t MEMORY[MEMORY_SIZE] = {0};
Registers REGS;
exitcode_t status;
FILE *log_file = NULL;

#ifdef BENCHMARK
    static uint64_t instr_count = 0;
    __clock_t time_spent;

    static inline void log_time(clock_t start, FILE* log)
    {
        clock_t diff = clock() - start;
        fprintf(log, "Speed calculated by clock(): %f\n", (float)diff / CLOCKS_PER_SEC);
    }
#endif



// Memory loading function.
void load_memory(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    
    // Always use protection.
    if (NULL == f)
    {
        perror("Error opening binary file");
        fprintf(stderr, "FATAL: Please ensure 'a.out.bin' exists and is accessible.\n");
        exit(EXIT_FAILURE); 
    }

    // Reads the entire 64K memory image written by the assembler
    // Yeah, this reads more than you this whole year.
    size_t words_read = fread(MEMORY, sizeof(word_t), MEMORY_SIZE, f);

    if (words_read > 0)
        printf("Loaded %zu words starting at 0x%04X.\n", words_read, CODE_START);
    else
        printf("Warning: Binary file loaded but appears empty lol.\n");

    fclose(f);
}



void run_simulator(int start_addr)
{
#   ifdef BENCHMARK
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
#   endif

    REGS.PC = start_addr;

    printf("** Starting Simulator at 0x%04X **\n", start_addr);

    // **Computed goto dispatch table**
    static void *dispatch_table[] = {
        [OP_ILLEGAL]   = &&op_illegal,
        [OP_ALU_LOGIC] = &&op_alu_logic,
        [OP_STACK_OPS] = &&op_stack_ops,
        [OP_BRANCH]    = &&op_branch,
        [OP_LDI]       = &&op_ldi,
        [OP_LOAD]      = &&op_load,
        [OP_STORE]     = &&op_store,
        [OP_JMP]       = &&op_jmp,
        [OP_JAL]       = &&op_jal,
        [OP_RET]       = &&op_ret,
        [OP_TRAP]      = &&op_trap,
        [OP_HALT]      = &&op_halt
    };

fetch:
#   ifdef BENCHMARK
    instr_count++;
#   endif

    // FETCH
    Instruction current_instruction;
    current_instruction.raw = MEMORY[REGS.PC];
    word_t prev_pc = REGS.PC;
    REGS.PC++;

#   ifndef HIDE_TRACE
#   ifndef NO_LOG
    fprintf(log_file,
        "PC: 0x%04X, SP: 0x%04X, Instruction: 0x%04X (opcode: 0x%X, arg: 0x%X)\n",
        prev_pc, REGS.SP, current_instruction.raw,
        current_instruction.fields.opcode, current_instruction.fields.arg);
#   endif
#   endif

    int opcode = current_instruction.fields.opcode;
    int arg    = current_instruction.fields.arg;

    if (__builtin_expect(opcode < OP_ILLEGAL || opcode > OP_HALT, 0))
        goto op_illegal;

    goto *dispatch_table[opcode];

op_illegal:
    fprintf(stderr, "Runtime Error: Illegal Opcode 0x%X at address 0x%04X\n", opcode, prev_pc);
    CLOSE_LOG();
    exit(EXIT_FAILURE);

op_alu_logic:
    execute_alu(arg);
    goto fetch;

op_stack_ops:
    execute_stack_op(arg);
    goto fetch;

op_branch:
    execute_branch(arg);
    goto fetch;

op_ldi:
    REGS.SP--;
    MEMORY[REGS.SP] = (word_t)sign_extend_12(arg);
    goto fetch;

op_load:
{
    CHECK_SP_OVERFLOW(1);
    word_t ea = REGS.BR + (word_t)sign_extend_12(arg);
    REGS.SP--;
    MEMORY[REGS.SP] = MEMORY[ea];
    goto fetch;
}

op_store:
{
    CHECK_SP_UNDERFLOW(1);
    word_t ea = REGS.BR + (word_t)sign_extend_12(arg);
    MEMORY[ea] = MEMORY[REGS.SP];
    REGS.SP++;
    goto fetch;
}

op_jmp:
{
    REGS.PC += sign_extend_12(arg);
    goto fetch;
}

op_jal:
{
    CHECK_SP_OVERFLOW(1);
    REGS.SP--;
    MEMORY[REGS.SP] = REGS.LR;
    REGS.LR = REGS.PC;
    REGS.PC += sign_extend_12(arg);
    goto fetch;
}

op_ret:
{
    CHECK_SP_UNDERFLOW(1);
    REGS.PC = REGS.LR;
    REGS.LR = MEMORY[REGS.SP];
    REGS.SP++;
    goto fetch;
}

op_trap:
{
    if (TRAP_TABLE[arg] != NULL)
    {
        TRAP_TABLE[arg]();
        if (arg == 2) // special exit trap
        {
#           ifdef BENCHMARK
#           ifndef NO_LOG
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                             (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
            fprintf(log_file,
                    "Instr: %llu, elapsed: %.9f s, IPS: %.2f M\n",
                    (unsigned long long)instr_count,
                    elapsed, instr_count / (elapsed * 1e6));
#           endif
#           endif
            printf("\n** SYS_EXIT at 0x%04X with status %hhu (0x%04X) **\n", prev_pc, status, status);
            CLOSE_LOG();
            exit(status);
        }
    }
    else
    {
        fprintf(stderr, "Runtime Error: Unknown TRAP code 0x%X at 0x%04X\n", arg, prev_pc);
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }
    goto fetch;
}

op_halt:
#   ifdef BENCHMARK
#   ifndef NO_LOG
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    fprintf(log_file,
            "Instr: %llu, elapsed: %.9f s, IPS: %.2f M\n",
            (unsigned long long)instr_count,
            elapsed, instr_count / (elapsed * 1e6));
#   endif
#   endif
    printf("\n** HALT at 0x%04X **\n", prev_pc);
    CLOSE_LOG();
    exit(EXIT_SUCCESS);
}



// The main program.
int main(void)
{
    // Initial setup...
    REGS.SP = INITIAL_SP;
    // Historical reminder that once we thought a static section was a good idea.
    // REGS.BR = 0x2000;   // We set the Base Register for data access start.

    initialize_trap_table();

    load_memory("a.out.bin");

#   ifndef NO_LOG
        log_file = fopen("pmv.log", "w");
#   endif

    if (NULL == log_file)
    {
        perror("FATAL: Could not open log file 'pmv.log'");
        return EXIT_FAILURE;
    }

    REGS.BR = MEMORY[0x0000];

    run_simulator(CODE_START);

    // We shouldn't be here.
    // Still... Just in case.
    CLOSE_LOG();
    return EXIT_SUCCESS;
}
