
#include "isa_defs.h"
#include "assembler_defs.h"
#include "string_utils.h"
#include <ctype.h>      // For isspace, among others...
#include <string.h>

word_t MEMORY[MEMORY_SIZE] = {0};
Registers REGS;

exitcode_t status = EXIT_SUCCESS;

const MnemonicMap INSTRUCTION_MAP[] = {
    // ALU (Format 1, uses FUNC_ code, no operand check needed).
    {"ADD",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_ADD,  0},
    {"SUB",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_SUB,  0},
    {"MULT", TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_MULT, 0},
    {"DIV",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_DIV,  0},
    {"NEG",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_NEG,  0},
    {"INC",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_INC,  0},
    {"DEC",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_DEC,  0},
    {"ABS",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_ABS,  0},
    {"NOT",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_NOT,  0},
    {"AND",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_AND,  0},
    {"OR",   TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_OR,   0},
    {"XOR",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_XOR,  0},
    {"SHL",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_SHL,  0},
    {"SHR",  TYPE_FORMAT1, OP_ALU_LOGIC, FUNC_SHR,  0},
    // STACK_OPS (Format 1, uses FUNC_ code, so no operand check needed).
    {"SWAP", TYPE_FORMAT1, OP_STACK_OPS, FUNC_SWAP, 0},
    {"DUP",  TYPE_FORMAT1, OP_STACK_OPS, FUNC_DUP,  0},
    {"DROP", TYPE_FORMAT1, OP_STACK_OPS, FUNC_DROP, 0},
    {"OVER", TYPE_FORMAT1, OP_STACK_OPS, FUNC_OVER, 0},
    // BRANCH (Format 1, uses FUNC_ code, no operand check needed).
    {"BEQ", TYPE_FORMAT1, OP_BRANCH, FUNC_BEQ, 0},
    {"BNE", TYPE_FORMAT1, OP_BRANCH, FUNC_BNE, 0},
    {"BZ",  TYPE_FORMAT1, OP_BRANCH, FUNC_BZ,  0},
    {"BNZ", TYPE_FORMAT1, OP_BRANCH, FUNC_BNZ, 0},
    {"BN",  TYPE_FORMAT1, OP_BRANCH, FUNC_BN,  0},
    {"BP",  TYPE_FORMAT1, OP_BRANCH, FUNC_BP,  0},
    // Control flow/memory (Format 2, requires_operand = 1).
    {"LDI",   TYPE_FORMAT2, OP_LDI,   0, 1},
    {"LOAD",  TYPE_FORMAT2, OP_LOAD,  0, 1},
    {"STORE", TYPE_FORMAT2, OP_STORE, 0, 1},
    {"JMP",   TYPE_FORMAT2, OP_JMP,   0, 1},
    {"JAL",   TYPE_FORMAT2, OP_JAL,   0, 1},
    // RET (Special case, no operand, uses format 1 with func=0).
    {"RET",   TYPE_NO_FUNC, OP_RET,   0, 0},
    // For System Calls (Format 2, requires_operand = 1).
    {"TRAP",  TYPE_FORMAT2, OP_TRAP,  0, 1},
    // HALT (Special case, no operand, uses format 1 with func=0).
    {"HALT", TYPE_NO_FUNC, OP_HALT, 0, 0}
};

const size_t INSTRUCTION_COUNT = sizeof(INSTRUCTION_MAP) / sizeof(INSTRUCTION_MAP[0]);

// The helper functions for encoding:



// For a Format 2 instruction,
// (Opcode + 12-bit immediate/offset/syscall code).
word_t encode_format2(int opcode, int operand)
{
    word_t encoded_op      = ((word_t)opcode) << 12;
    word_t encoded_operand = ((word_t)operand) & 0x0FFF;
    return encoded_op | encoded_operand;
}



// For a Format 1 instruction,
// (Opcode + 12-bit function code)
word_t encode_format1(int opcode, int func)
{
    word_t encoded_op    = ((word_t)opcode) << 12;
    word_t encoded_func = ((word_t)func) & 0x0FFF;
    return encoded_op | encoded_func;
}



// The main assembler logic.
int main(int argc, char **argv)
{
    // We could change this once we can actually assemble 
    // more than one .asm file per time, but that's gonna take
    // some work. Still, I will add a TODO for this possible
    // future ticket, you know :/.
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <assembly_file.asm>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // THE SETUPPPPP
    FILE *input = fopen(argv[1], "r");
    if (!input)
    {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    // Originally it was fixed, but to make tiny binaries
    // we will calculate it dynamically.
    // REGS.BR = 0x2000;

    word_t current_address = CODE_START;
    int data_mode = 0;  // CODE = 0, DATA = 1 

    word_t max_address_written = 0;

    char line[512];
    char mnemonic[16];
    char operand_str[100];
    int  operand = 0;

    MnemonicMap *map_entry = NULL;

    fprintf(stdout, "** Assembling %s **\n", argv[1]);

    // Time to build the memory image.
    while(fgets(line, sizeof(line), input))
    {
        // # = comments, so we forget about them.
        // Okay, ';' will also be considered a token for comments.
        char *hash_comment = strchr(line, '#');
        char *semicolon_comment = strchr(line, ';');

        char *comment_start = NULL;

        if (hash_comment && semicolon_comment)
        {
            comment_start = (hash_comment < semicolon_comment) ? hash_comment : semicolon_comment;
        } 
        else if (hash_comment)
        {
            comment_start = hash_comment;
        }
        else if (semicolon_comment)
        {
            comment_start = semicolon_comment;
        }
        if (comment_start)
        {
            *comment_start = '\0';
        }

        char *p = line;

        // Time to manage the labels (LABEL:)
        char *label_end = strchr(p, ':');
        if (label_end)
        {
            // If there's a label, we start parsing AFTER the ':'
            p = label_end + 1;
        }

        // We don't care about spaces.
        while (*p && isspace(*p))
        {
            p++;
        }

        if ('\0' == *p)
        {
            continue;
        }

        // Parse the mnemonic and the operand.
        int fields = sscanf(p, "%15s %99s", mnemonic, operand_str);

        if (fields < 1)
        {
            continue;
        }

        // So, you can use different bases.
        // By default you can use decimal, hexadecimal and octal.
        // Binary is not a default format, so that is why this exists.
        // Pretty trivial, strtol does the magic.
        // https://man7.org/linux/man-pages/man3/strtol.3.html
        if (2 == fields)
        {
            if (0 == strncmp(operand_str, "0b", 2))
            {
                operand = (int)strtol(operand_str + 2, NULL, 2);
            }
            else
            {
                operand = (int)strtol(operand_str, NULL, 0);
            }
        }

        if (0 == strcmp(mnemonic, ".DATA"))
        {
            data_mode = 1;
            REGS.BR = current_address;
            continue;
        }
        else if (0 == strcmp(mnemonic, ".CODE"))
        {
            data_mode = 0;
            continue;
        }
        if (1 == data_mode)
        {
            // The data...
            if (0 == strcmp(mnemonic, ".WORD"))
            {
                if (fields < 2)
                {
                    fprintf(stderr, "Error: .WORD missing value at address 0x%04X\n", current_address);
                    status = EXIT_FAILURE;
                    break;
                }

                MEMORY[current_address++] = (word_t)operand;
                
                if (current_address > max_address_written)
                {
                    max_address_written = current_address;
                }

            }
            else if (0 == strcmp(mnemonic, ".STRING"))
            {
                char *start_quote = strchr(p, '"'); // We search the " after the mnemonic .STRING
                if (!start_quote)
                {
                    fprintf(stderr, "Error: .STRING missing opening quote.\n");
                    status = EXIT_FAILURE;
                    break;
                }
                start_quote++;
                char *end_quote = strchr(start_quote, '"');
                if (!end_quote)
                {
                    fprintf(stderr, "Error: .STRING missing closing quote.\n");
                    status = EXIT_FAILURE;
                    break;
                }
                *end_quote = '\0'; 
                
                word_t words_used = str_pack(start_quote, current_address);

                current_address += words_used;

                if (current_address > max_address_written)
                {
                    max_address_written = current_address;
                }
            }
            else
            {
                fprintf(stderr, "Error: Unknown data directive: %s\n", mnemonic);
                status = EXIT_FAILURE;
                break;
            }
        } 
        else
        {
            // The instructions...
            word_t instruction_word = 0;
            
            map_entry = NULL;

            // We search the lookup table.
            for (size_t i = 0; i < INSTRUCTION_COUNT; ++i)
            {
                if (0 == strcmp(mnemonic, INSTRUCTION_MAP[i].mnemonic))
                {
                    map_entry = (MnemonicMap *)&INSTRUCTION_MAP[i];
                }
            }

            if (!map_entry)
            {
                fprintf(stderr, "Error: Unknown instruction mnemonic '%s' at address 0x%04X\n", mnemonic, current_address);
                status = EXIT_FAILURE;
                instruction_word = encode_format1(OP_ILLEGAL, 0); 
            }
            else
            {
                if (map_entry->requires_operand)
                {
                    // Operand is required (TYPE_FORMAT2).
                    if (fields < 2) 
                    {
                        // Error: missing operand.
                        fprintf(stderr, "Error: Instruction %s missing operand at address 0x%04X\n", mnemonic, current_address);
                        status = EXIT_FAILURE;
                        instruction_word = encode_format1(OP_ILLEGAL, 0); 
                    }  
                    else 
                    {
                        // Warning for 12-bit truncation (user requested change).
                        // Range checked: [-2048, 4095] to cover both signed and unsigned 12-bit limits.
                        if (operand > 4095 || operand < -2048)
                        {
                            word_t truncated_operand = ((word_t)operand) & 0x0FFF;
                            fprintf(stderr, "Warning: Operand value '%d' for instruction '%s' at address 0x%04X exceeds 12-bit range. It will be truncated to %u (0x%03X).\n",
                                    operand, mnemonic, current_address, truncated_operand, truncated_operand);                        }
                        
                        // Encoding time!
                        instruction_word = encode_format2(map_entry->opcode, operand); 
                    }
                }
                else // Operand not required (TYPE_FORMAT1, TYPE_NO_FUNC).
                {
                    if (fields >= 2) 
                    {
                        // Warning for extraneous operand.
                        fprintf(stderr, "Warning: Operand '%d' for instruction '%s' at address 0x%04X will be ignored.\n", operand, mnemonic, current_address);
                    }
                    
                    // Encoding time...
                    switch (map_entry->type) 
                    {
                        case TYPE_FORMAT1:
                            instruction_word = encode_format1(map_entry->opcode, map_entry->func_code);
                            break;
                        case TYPE_FORMAT2:
                            // Should not happen as requires_operand is 0.
                            instruction_word = encode_format1(OP_ILLEGAL, 0); 
                            break;
                        case TYPE_NO_FUNC:
                            // FUNC is 0 for HALT and RET
                            instruction_word = encode_format1(map_entry->opcode, 0); 
                            break;
                    }
                }
            }
            MEMORY[current_address++] = instruction_word;

            if (current_address > max_address_written)
            {
                max_address_written = current_address;
            }
        }
    }

    // The second pass is to write the binary output.

    // TODO: Add error checking.

    // This allows the simulator to know where the data section starts.
    MEMORY[0x0000] = REGS.BR; 

    word_t total_words_used = max_address_written;

    // We ensure 0x0000 is included even if no code/data was written
    if (total_words_used < 1)
    {
        total_words_used = 1;
    }

    FILE *output = fopen("a.out.bin", "wb");
    if (!output)
    {
        perror("Error opening output file");
        fclose(input);
        return EXIT_FAILURE;
    }

    size_t words_written = fwrite(MEMORY, sizeof(word_t), total_words_used, output);

    fclose(input);
    fclose(output);
    
    fprintf(stdout, "Assembly complete. Wrote memory image (%zu bytes) to a.out.bin\n", 
            words_written * sizeof(word_t));

    return status;
}
