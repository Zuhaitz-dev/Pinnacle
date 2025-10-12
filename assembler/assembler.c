
#include "isa_defs.h"
#include "assembler_defs.h"
#include "string_utils.h"
#include <ctype.h>      // For isspace, among others...
#include <string.h>

// Global defintion of memory and registers.
// We use it here to build the memory image :p.
word_t MEMORY[MEMORY_SIZE] = {0};
Registers REGS;

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



// THis would pack a string into memory, 2 characters per word, null-terminated.
// The first char is the High Byte, the second is the Low Byte.
// Returns the number of words written.
word_t pack_string(const char *str, word_t start_address)
{
    size_t len = strlen(str);
    word_t words_written = 0;

    // First, write the length of the string as the first word.
    MEMORY[start_address] = (word_t)len;
    words_written++;

    // Loop through the characters, incrementing by 2 each time.
    // We loop up to len because the final iteration is needed to handle the
    // explicit null-terminator word (0x0000) if the string has an even length.
    for (size_t i = 0; i < len; i += 2)
    {
        
        char c1 = str[i];
        char c2 = (i + 1 < len) ? str[i + 1] : 0;
        
        // If i == len (even length string), c1 and c2 are '\0', resulting in 0x0000.
        // If i == len - 1 (odd length string), c1 is the last char, c2 is '\0'.
        word_t packed_word = ((word_t)c1 << 8) | (word_t)c2;
        
        // We write the packed string in memory.
        MEMORY[start_address + words_written] = packed_word;
        words_written++;
    }
    
    return words_written;
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
    int operand;

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
        int fields = sscanf(p, "%15s %i", mnemonic, &operand);

        if (fields < 1)
        {
            continue;
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
                    break;
                }
                start_quote++;
                char *end_quote = strchr(start_quote, '"');
                if (!end_quote)
                {
                    fprintf(stderr, "Error: .STRING missing closing quote.\n");
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
                fprintf(stderr, "Unknown data directive: %s\n", mnemonic);
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
                instruction_word = encode_format1(OP_ILLEGAL, 0); 
            }
            else
            {
                if (map_entry->requires_operand && fields < 2) 
                {
                    fprintf(stderr, "Instruction %s missing operand at address 0x%04X\n", mnemonic, current_address); 
                    // We can skip writing the instruction, but writing an ILLEGAL one seems better.
                    instruction_word = encode_format1(OP_ILLEGAL, 0); 
                }  
                else 
                {
                    // Encoding time!
                    switch (map_entry->type) 
                    {
                        case TYPE_FORMAT1:
                            instruction_word = encode_format1(map_entry->opcode, map_entry->func_code);
                            break;
                        case TYPE_FORMAT2:
                            // operand is already parsed into an integer
                            instruction_word = encode_format2(map_entry->opcode, operand); 
                            break;
                        case TYPE_NO_FUNC:
                            // FUNC is 0 for HALT and ILLEGAL
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

    return EXIT_SUCCESS;
}
