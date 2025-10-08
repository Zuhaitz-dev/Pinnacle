#define _POSIX_C_SOURCE 199309L

#include "isa_defs.h"
#include <stdlib.h>
#include <unistd.h>     // We need this one for POSIX write().
#include <fcntl.h>      // And we need this other one for POSIX open().

#ifdef BENCHMARK
#   include <time.h>
#endif



// Global definition of memory and registers.
word_t MEMORY[MEMORY_SIZE] = {0};
Registers REGS;
exitcode_t status;

#ifndef NO_LOG
    FILE *log_file;
#endif

#ifdef BENCHMARK
    static uint64_t instr_count = 0;
    __clock_t time_spent;
#endif

// Global TRAP Dispatch Table definition.
trap_handler_t TRAP_TABLE[256] = {0};



#ifdef BENCHMARK
static inline void log_time(clock_t start, FILE* log)
{
    clock_t diff = clock() - start;
    fprintf(log, "Speed calculated by clock(): %f\n", (float)diff / CLOCKS_PER_SEC);
}
#endif



// Stack macros.
#ifdef BENCHMARK
#   define CHECK_SP_OVERFLOW(n) ((void)0)
#   define CHECK_SP_UNDERFLOW(n) ((void)0)
#else
#   define CHECK_SP_UNDERFLOW(n) if (REGS.SP + n > INITIAL_SP) { fprintf(stderr, "Stack underflow\n"); exit(EXIT_FAILURE); }
#   define CHECK_SP_OVERFLOW(n)  if (REGS.SP < n) { fprintf(stderr, "Stack overflow\n"); exit(EXIT_FAILURE); }
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



string_t unpack_string(word_t buf_offset, word_t count) {
    word_t addr = REGS.BR + buf_offset;
    char *tmp = malloc(count + 1);
    if (!tmp) { perror("malloc"); exit(EXIT_FAILURE); }

    word_t actual_len = count;
    for (word_t i = 0; i < count; i++)
    {
        char c = GET_CHAR_FROM_WORD(MEMORY[addr + i / 2], i);
        tmp[i] = c;
        if (c == '\0') { actual_len = i; break; }
    }
    tmp[actual_len] = '\0';

    string_t res = { .str = tmp, .count = actual_len };
    return res;
}



// Syscall 0: read(fd, buf_offset, count);
void trap_read_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // Okay, this is pretty much like SYS_WRITE in the sense that
    // the arguments are pretty much the same.
    // (Yes, even if this appears first, I think, I implemented SYS_WRITE first).
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    char tmp[count];
    ssize_t n = read((int)fd, tmp, count);

    if (n <= 0)
    {
        MEMORY[--REGS.SP] = (word_t)n;
        return;
    }

    word_t addr = REGS.BR + buf_offset;
    for (ssize_t i = 0; i < n; i++)
    {
        word_t idx = i / 2;
        if (0 == i % 2)
            MEMORY[addr + idx] = (tmp[i] & 0xFF) << 8; 
        else
            MEMORY[addr + idx] |= (tmp[i] & 0xFF);
    }

    MEMORY[--REGS.SP] = (word_t)n;
}



// Syscall 1: write(fd, buf_offset, count);
void trap_write_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // The parameters are popped from the stack in the reverse order they were pushed.
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (which is relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    string_t write_str = unpack_string(buf_offset, count);
    write((fd==1?STDOUT_FILENO:(fd==2?STDERR_FILENO:fd)), write_str.str, write_str.count);
    free(write_str.str);
    MEMORY[--REGS.SP] = write_str.count;
}



// Syscall 2: exit(status);
void trap_exit_handler()
{
    // The exitcode is stored at the Base Register.
    status = MEMORY[REGS.BR];
}



// Syscall 3: open(*path, flags, count);
// Count is added as a way to actually unpack this.
// There are other options, like mode, but let's work with this.
void trap_open_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // POP count. If not, good luck.
    word_t count = MEMORY[REGS.SP++];
    // POP Flags. Check: https://man7.org/linux/man-pages/man2/open.2.html
    word_t flags = MEMORY[REGS.SP++];
    // POP path. We still have to process it.
    word_t buf_offset = MEMORY[REGS.SP++];

    string_t open_str = unpack_string(buf_offset, count);
    int fd = open(open_str.str, (int)flags);

    // ALWAYS FREE.
    free(open_str.str);
    MEMORY[--REGS.SP] = (word_t)fd;
}



// Syscall 4: 
void trap_close_handler()
{
    // TODO: Add definition.
}



// We have 256 options, could we expand this later if so? Yeah?
void initialize_trap_table()
{
    // For now we have this one.
    // TODO: Add the implementation for more system calls.
    TRAP_TABLE[0] = trap_read_handler;
    TRAP_TABLE[1] = trap_write_handler;
    TRAP_TABLE[2] = trap_exit_handler;
    TRAP_TABLE[3] = trap_open_handler;
    TRAP_TABLE[4] = trap_close_handler;
}



sword_t sign_extend_12(int val)
{
    if (val & 0x0800)
        return (sword_t)(val | 0xF000);
    return (sword_t)val;
}



// ALU execution logic (let's cry).
static inline void execute_alu(int func_code)
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
            exit(EXIT_FAILURE);
    }
}



static inline void execute_stack_op(int func_code)
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
            exit(EXIT_FAILURE);
    }
}



static inline void execute_branch(int func_code)
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
            exit(EXIT_FAILURE);
    }
}



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
            return;
        }
    }
    else
    {
        fprintf(stderr, "Runtime Error: Unknown TRAP code 0x%X at 0x%04X\n", arg, prev_pc);
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
    return;
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

    if (REGS.SP < INITIAL_SP)
        printf("Final TOS value: %d (0x%04X)\n", (sword_t)MEMORY[REGS.SP], MEMORY[REGS.SP]);
    printf("Value at BR (Data start 0x%04X): %d (0x%04X)\n", REGS.BR, (sword_t)MEMORY[REGS.BR], MEMORY[REGS.BR]);

#   ifndef NO_LOG
    fclose(log_file);
#   endif

    return EXIT_SUCCESS;
}
