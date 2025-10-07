
#include "isa_defs.h"
#include <stdlib.h>
#include <unistd.h>     // We need this one for POSIX write().
#include <fcntl.h>      // And we need this other one for POSIX open().

// Global definition of memory and registers.
word_t MEMORY[MEMORY_SIZE] = {0};
Registers REGS;

// Global TRAP Dispatch Table definition.
trap_handler_t TRAP_TABLE[256] = {0};



// Helper function.
string_t unpack_string(word_t buf_offset, word_t count)
{
    string_t unpacked_str;

    // Calculate the starting address in the words array.
    word_t string_start_addr_words = REGS.BR + buf_offset;

    // We need a temporary buffer to hold the characters we extract.
    // The max size is 'count' characters + 1 for a potential newline + 1 for a C-string null terminator.
    char *temp_buffer = (char*)malloc(count + 2); 
    if (NULL == temp_buffer)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    word_t actual_length = count; // To track the length before hitting a null.

    // Loop through the *characters* (i), extracting 2 chars per word from memory.
    for (int i = 0; i < count; i++)
    {
        // Calculate the word address (character index / 2).
        word_t word_index = i / 2;
        word_t current_word = MEMORY[string_start_addr_words + word_index];

        char extracted_char;
        if (0 == (i % 2))
        {
            // Index 0, 2, 4, ...: First character of the pair (High Byte).
            // Shift right by 8 bits to move the high byte to the low byte position, then mask.
            extracted_char = (char)((current_word >> 8) & 0xFF);
        }
        else
        {
            // Index 1, 3, 5, ...: Second character of the pair (Low Byte).
            // Simply mask the low 8 bits.
            extracted_char = (char)(current_word & 0xFF);
        }

        temp_buffer[i] = extracted_char;

        // Check for null terminator (important for .STRING).
        if ('\0' == extracted_char)
        {
            actual_length = i; // The actual length is up to the null terminator.
            break;
        }
    }

    unpacked_str.count = actual_length;
    
    // Always null-terminate the buffer for safety, even if we write a newline.
    temp_buffer[unpacked_str.count] = '\0';
    unpacked_str.str = temp_buffer;

    return unpacked_str;
}


// Syscall 0: read(fd, buf_offset, count);
void trap_read_handler()
{
    // Okay, this is pretty much like SYS_WRITE in the sense that
    // the arguments are pretty much the same.
    // (Yes, even if this appears first, I think, I implemented SYS_WRITE first).
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    int host_fd = (int)fd;
    char temp_buffer[count]; 
    ssize_t bytes_read = read(host_fd, temp_buffer, count);

    if (bytes_read <= 0)
    {
        // Push the result.
        // Here we can expect 0 for EOF, -1 for error.
        MEMORY[--REGS.SP] = (word_t)bytes_read;
        return;
    }

    word_t string_start_addr_words = REGS.BR + buf_offset;

    word_t current_word_value = 0;
    word_t word_index = 0;

    for (ssize_t i = 0; i < bytes_read; i++) 
    {
        char incoming_char = temp_buffer[i];
        
        if (0 == (i % 2)) 
        {
            // High Byte.
            word_index = i / 2;
            current_word_value = ((word_t)incoming_char << 8) & 0xFF00;
        } 
        else 
        {
            // Low Byte.
            current_word_value |= ((word_t)incoming_char & 0x00FF);
            
            // Write to memory as the word is complete now.
            MEMORY[string_start_addr_words + word_index] = current_word_value;
            current_word_value = 0; // We reset this for next word.
        }
    }

    if (bytes_read % 2 != 0)
    {
        // If the number of bytes read is an odd number, the word would be incomplete.
        // To prevent losing information, we will just write the partially written word.
        MEMORY[string_start_addr_words + word_index] = current_word_value;
    }

    MEMORY[--REGS.SP] = (word_t)bytes_read; 
}



// Syscall 1: write(fd, buf_offset, count);
void trap_write_handler()
{
    // The parameters are popped from the stack in the reverse order they were pushed.
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (which is relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    string_t write_str = unpack_string(buf_offset, count);
    word_t write_len = write_str.count;
    char *temp_buffer = write_str.str;

    // Write to the file descriptor (STDOUT or STDERR).
    if (1 == fd || 2 == fd) 
    {
        write(fd == 1 ? STDOUT_FILENO : STDERR_FILENO, temp_buffer, write_len);
    }
    // else, handle other file descriptors.
    else
    {
        write(fd, temp_buffer, write_len);
    }

    // ALWAYS FREE.
    free(write_str.str);

    // The result value (number of bytes written) is pushed to the stack.
    MEMORY[--REGS.SP] = write_len; // Return the number of characters, excluding newline.
}



// Syscall 2: exit(status);
void trap_exit_handler()
{
    // The exitcode is stored at the Base Register.
    exitcode_t status = MEMORY[REGS.BR];

    printf("\n** SYS_EXIT with status %hhu (0x%04X) **\n", status, status);

    if (REGS.SP < INITIAL_SP)
    {
        printf("Final TOS value: %d (0x%04X)\n", (sword_t)MEMORY[REGS.SP], MEMORY[REGS.SP]);
    }
    // We are going to make 0x2000 as the "exit code word".
    // Therefore showing status or the value at BR is just the same.
    // printf("Value at BR (Data start 0x%04X): %d (0x%04X)\n", REGS.BR, (sword_t)MEMORY[REGS.BR], MEMORY[REGS.BR]);

    exit((int)status);
}



// Syscall 3: open(*path, flags, count);
// Count is added as a way to actually unpack this.
// There are other options, like mode, but let's work with this.
void trap_open_handler()
{
    // POP count. If not, good luck.
    word_t count = MEMORY[REGS.SP++];
    // POP Flags. Check: https://man7.org/linux/man-pages/man2/open.2.html
    word_t flags = MEMORY[REGS.SP++];
    // POP path. We still have to process it.
    word_t buf_offset = MEMORY[REGS.SP++];

    string_t open_str = unpack_string(buf_offset, count);
    char* temp_buffer = open_str.str;

    int host_fd = open(temp_buffer, (int)flags);

    // ALWAYS FREE.
    free(open_str.str);

    MEMORY[--REGS.SP] = (word_t)host_fd;
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



// Helper functions.
sword_t sign_extend_12(int val)
{
    if (val & 0x0800)
    {
        return (sword_t)(val | 0xF000);
    }
    return (sword_t)val;
}



// ALU execution logic (let's cry).
void execute_alu(int func_code)
{
    if (REGS.SP >= INITIAL_SP) { 
        fprintf(stderr, "Runtime Error: Stack underflow for ALU operation.\n");
        exit(EXIT_FAILURE);
    }

    // Top of stack (TOS) and Next on stack (NOS)
    word_t w_tos = MEMORY[REGS.SP];
    word_t w_nos = MEMORY[REGS.SP + 1];
    sword_t s_tos = (sword_t)w_tos;
    sword_t s_nos = (sword_t)w_nos;

    sword_t s_result = 0;
    word_t  w_result = 0;

    switch (func_code)
    {
        // **Arithmetic (binary)**
        case FUNC_ADD:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on ADD.\n");
                exit(EXIT_FAILURE);
            }
            s_result = s_nos + s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            REGS.SP++;
            break;

        case FUNC_SUB:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on SUB.\n");
                exit(EXIT_FAILURE);
            }
            s_result = s_nos - s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            REGS.SP++;
            break;

        case FUNC_MULT:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on MULT.\n");
                exit(EXIT_FAILURE);
            }
            s_result = s_nos * s_tos;
            MEMORY[REGS.SP + 1] = (word_t)s_result;
            REGS.SP++;
            break;

        case FUNC_DIV:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on DIV.\n");
                exit(EXIT_FAILURE);
            }
            if (0 == s_tos)
            {
                fprintf(stderr, "Divide by zero.\n");
                exit(EXIT_FAILURE);
            }
            s_result = s_nos / s_tos;
            MEMORY[REGS.SP + 1] = (word_t)(s_nos % s_tos); // remainder
            MEMORY[REGS.SP]     = (word_t)s_result;        // quotient
            break;

        // **Arithmetic (unary)**
        case FUNC_NEG:
            MEMORY[REGS.SP] = (word_t)(-s_tos);
            s_result = -s_tos;
            break;

        case FUNC_INC:
            MEMORY[REGS.SP] = (word_t)(s_tos + 1);
            s_result = s_tos + 1;
            break;

        case FUNC_DEC:
            MEMORY[REGS.SP] = (word_t)(s_tos - 1);
            s_result = s_tos - 1;
            break;

        case FUNC_ABS:
            s_result = (s_tos < 0 ? -s_tos : s_tos);
            MEMORY[REGS.SP] = (word_t)s_result;
            break;

        // **Logic**
        case FUNC_NOT:
            w_result = !w_tos;
            MEMORY[REGS.SP] = w_result;
            s_result = (sword_t)w_result;
            break;

        case FUNC_AND:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on AND.\n");
                exit(EXIT_FAILURE);
            }
            w_result = w_nos & w_tos;
            MEMORY[REGS.SP + 1] = w_result;
            REGS.SP++;
            s_result = (sword_t)w_result;
            break;

        case FUNC_OR:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on OR.\n");
                exit(EXIT_FAILURE);
            }
            w_result = w_nos | w_tos;
            MEMORY[REGS.SP + 1] = w_result;
            REGS.SP++;
            s_result = (sword_t)w_result;
            break;

        case FUNC_XOR:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on XOR.\n");
                exit(EXIT_FAILURE);
            }
            w_result = w_nos ^ w_tos;
            MEMORY[REGS.SP + 1] = w_result;
            REGS.SP++;
            s_result = (sword_t)w_result;
            break;

        case FUNC_SHL:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on SHL.\n");
                exit(EXIT_FAILURE);
            }
            w_result = w_nos << s_tos;
            MEMORY[REGS.SP + 1] = w_result;
            REGS.SP++;
            s_result = (sword_t)w_result;
            break;

        case FUNC_SHR:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Underflow on SHR.\n");
                exit(EXIT_FAILURE);
            }
            w_result = w_nos >> s_tos;
            MEMORY[REGS.SP + 1] = w_result;
            REGS.SP++;
            s_result = (sword_t)w_result;
            break;

        // Default cases are good practices, but this is not necessary in this case.
        // In compile time we already categorize unknown OPCODES as FUNC_ILLEGAL,
        // therefore we will not catch any unknown func codes in run time.
        default:
            fprintf(stderr, "Runtime Error: Unknown ALU func code 0x%X\n", func_code);
            exit(EXIT_FAILURE);
    }
}



void execute_stack_op(int func_code)
{
    word_t w_tos = MEMORY[REGS.SP];
    word_t w_nos = MEMORY[REGS.SP + 1];

    switch(func_code)
    {
        case FUNC_SWAP:
            if (REGS.SP >= INITIAL_SP - 1)
            {
                fprintf(stderr, "Stack Underflow on SWAP.\n");
                exit(EXIT_FAILURE);
            }
            MEMORY[REGS.SP]     = w_nos;
            MEMORY[REGS.SP + 1] = w_tos;
            break;

        case FUNC_DUP:
            if(0 == REGS.SP)
            {
                fprintf(stderr, "Stack Overflow on DUP.\n");
                exit(EXIT_FAILURE);
            }
            MEMORY[--REGS.SP] = w_tos;
            break;

        case FUNC_DROP:
            if(REGS.SP >= INITIAL_SP)
            {
                fprintf(stderr, "Stack Underflow on DROP.\n");
                exit(EXIT_FAILURE);
            }
            REGS.SP++;
            break;

        case FUNC_OVER:
            if(0 == REGS.SP)
            {
                fprintf(stderr, "Stack Overflow on OVER.\n");
                exit(EXIT_FAILURE);
            }
            MEMORY[--REGS.SP] = w_nos;
            break;

        default:
            fprintf(stderr, "Unknown stack func 0x%X\n", func_code);
            exit(EXIT_FAILURE);
    }
}



void execute_branch(int func_code) 
{
    switch (func_code)
    {
        case FUNC_BEQ:
        {
            if (REGS.SP > INITIAL_SP - 2)
            {
                fprintf(stderr, "Stack Underflow on BEQ.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];      // TOS = branch offset
            word_t rhs    = MEMORY[REGS.SP + 1];  // NOS = right operand
            word_t lhs    = MEMORY[REGS.SP + 2];  // N2OS = left operand

            REGS.SP += 3;



            if (lhs == rhs)
            {
                REGS.PC += sign_extend_12(offset);
            }
            break;
        }

        case FUNC_BNE:
        {
            if (REGS.SP > INITIAL_SP - 2)
            {
                fprintf(stderr, "Stack Underflow on BNE.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];
            word_t rhs    = MEMORY[REGS.SP + 1];
            word_t lhs    = MEMORY[REGS.SP + 2];

            REGS.SP += 3;

            if (lhs != rhs)
            {
                REGS.PC += sign_extend_12(offset);
            }
            break;
        }

        case FUNC_BZ:
        {
            if (REGS.SP > INITIAL_SP - 1)
            {
                fprintf(stderr, "Stack Underflow on BZ.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];

            REGS.SP += 2;

            if (0 == val)
            {
                REGS.PC += sign_extend_12(offset);
            }
            break;
        }

        case FUNC_BNZ:
        {
            if (REGS.SP > INITIAL_SP - 1)
            {
                fprintf(stderr, "Stack Underflow on BNZ.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];

            REGS.SP += 2;

            if (0 != val)
            {
                REGS.PC += sign_extend_12(offset);
            }
            break;
        }

        case FUNC_BN:
        {
            if (REGS.SP > INITIAL_SP - 1)
            {
                fprintf(stderr, "Stack Underflow on BN.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];

            REGS.SP += 2;

            if ((sword_t)val < 0)
            {
                REGS.PC += sign_extend_12(offset);
            }
            break;
        }

        case FUNC_BP:
        {
            if (REGS.SP > INITIAL_SP - 1)
            {
                fprintf(stderr, "Stack Underflow on BP.\n");
                exit(EXIT_FAILURE);
            }

            word_t offset = MEMORY[REGS.SP];
            word_t val    = MEMORY[REGS.SP + 1];

            REGS.SP += 2;

            if ((sword_t)val > 0)
            {
                REGS.PC += sign_extend_12(offset);
            }
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
    {
        printf("Loaded %zu words starting at 0x%04X.\n", words_read, CODE_START);
    }
    else
    {
        printf("Warning: Binary file loaded but appears empty lol.\n");
    }

    fclose(f);
}



// Main simulation loop yippee.
void run_simulator(int start_addr, FILE *log)
{
    REGS.PC = start_addr;

    printf("** Starting Simulator at 0x%04X **\n", start_addr);

    while (1)
    {
        // FETCH
        Instruction current_instruction;
        current_instruction.raw = MEMORY[REGS.PC];
        word_t prev_pc = REGS.PC;
        REGS.PC++;

        fprintf(log, "PC: 0x%04X, SP: 0x%04X, Instruction: 0x%04X (opcode: 0x%X, arg: 0x%X)\n",
            prev_pc, REGS.SP, current_instruction.raw, current_instruction.fields.opcode, current_instruction.fields.arg);

        // DECODE (time to use that union we made because tomatoes).
        int opcode = current_instruction.fields.opcode;
        int arg    = current_instruction.fields.arg;

        // EXECUTE 
        switch (opcode)
        {
            case OP_ILLEGAL:
                fprintf(stderr, "Runtime Error: Illegal Instruction. Did you write the instruction correctly?\n");
                exit(EXIT_FAILURE);

            case OP_ALU_LOGIC:
                execute_alu(arg);
                break;

            case OP_STACK_OPS:
                execute_stack_op(arg);
                break;

            case OP_BRANCH:
                execute_branch(arg);
                break;

            case OP_LDI: // LDI Immediate.
                REGS.SP--;
                MEMORY[REGS.SP] = (word_t)sign_extend_12(arg);
                break;
        
            case OP_LOAD: // LOAD offset.
            {
                if (0 == REGS.SP)
                {
                    fprintf(stderr, "Stack Overflow.\n");   // Fun fact, Stack Overflow was named after this ISA.
                    exit(EXIT_FAILURE);
                }
                word_t ea = REGS.BR + (word_t)sign_extend_12(arg);
                REGS.SP--;
                MEMORY[REGS.SP] = MEMORY[ea];
                break;
            }

            case OP_STORE: // STORE offset.
            {
                if (REGS.SP >= INITIAL_SP)
                {
                    fprintf(stderr, "Stack Underflow.\n");  // The evil cousin of Stack Overflow.
                    exit(EXIT_FAILURE);
                }
                word_t ea = REGS.BR + (word_t)sign_extend_12(arg);
                MEMORY[ea] = MEMORY[REGS.SP];
                REGS.SP++;
                break;
            }

            case OP_JMP: // JMP offset.
            {
                sword_t offset = sign_extend_12(arg);
                REGS.PC += offset;
                break;
            }
            
            case OP_JAL: // Jump and Link
            {
                if (0 == REGS.SP)
                {
                    fprintf(stderr, "Stack Overflow on JAL.\n");
                    exit(EXIT_FAILURE);
                }
                REGS.SP--; 
                MEMORY[REGS.SP] = REGS.LR; 

                // Update LR (address will be PC before jump)
                REGS.LR = REGS.PC; 

                // Now this is like JMP
                sword_t offset = sign_extend_12(arg);
                REGS.PC += offset;
                break;
            }

            case OP_RET:
            {
            // POP LR
            if (REGS.SP >= INITIAL_SP)
                {
                    fprintf(stderr, "Stack Underflow on RET.\n");
                    exit(EXIT_FAILURE);
                }
                REGS.PC = REGS.LR; // PC jumps back to LR
                REGS.LR = MEMORY[REGS.SP]; 
                REGS.SP++; 
                break;
            }

            case OP_TRAP:
            {
                // Use the argument, the syscall code, as an index into the table
                if (TRAP_TABLE[arg] != NULL)
                {
                    TRAP_TABLE[arg](); // Execute whatever it actually is.
                }
                else    // We are cooked if you are here.
                {
                    fprintf(stderr, "Runtime Error: Unknown TRAP code 0x%X at 0x%04X\n", arg, prev_pc);
                    exit(EXIT_FAILURE);
                }
                break;
            }

            case OP_HALT:
                printf("\n** HALT at 0x%04X **\n", prev_pc);
                return;

            default:
                fprintf(stderr, "Runtime Error: Invalid Opcode 0x%X at address 0x%04X\n", opcode, prev_pc);
                exit(EXIT_FAILURE);
        }
    }
}



// The main program.
int main(void)
{
    // Initial setup...
    REGS.SP = INITIAL_SP;
    // REGS.BR = 0x2000;   // We set the Base Register for data access start.

    initialize_trap_table();

    load_memory("a.out.bin");

    FILE *log = fopen("pmv.log", "w");
    if (NULL == log)
    {
        perror("FATAL: Could not open log file 'pmv.log'");
        return EXIT_FAILURE;
    }

    REGS.BR = MEMORY[0x0000];

    run_simulator(CODE_START, log);

    if (REGS.SP < INITIAL_SP)
    {
        printf("Final TOS value: %d (0x%04X)\n", (sword_t)MEMORY[REGS.SP], MEMORY[REGS.SP]);
    }
    printf("Value at BR (Data start 0x%04X): %d (0x%04X)\n", REGS.BR, (sword_t)MEMORY[REGS.BR], MEMORY[REGS.BR]);

    fclose(log);

    return EXIT_SUCCESS;
}
