
#include "isa_defs.h"
#include "assembler_defs.h"
#include "string_utils.h"
#include <ctype.h>      // For isspace, among others...
#include <string.h>
#include <errno.h>

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

Symbol SYMBOL_TABLE[MAX_SYMBOLS];
int symbol_count = 0;
word_t data_section_start_address = 0;

void add_symbol(const char* name, word_t address)
{
    if (symbol_count >= MAX_SYMBOLS)
    {
        fprintf(stderr, "Error: Symbol table full. Max %d symbols.\n", MAX_SYMBOLS);
        status = EXIT_FAILURE;
        return;
    }    

    for (int i = 0; i < symbol_count; ++i)
    {
        if (0 == strcmp(SYMBOL_TABLE[i].name, name))
        {
            fprintf(stderr, "Error: Duplicate label definition '%s' at 0x%04X\n", name, address);
            status = EXIT_FAILURE;
            return;
        }
    }

    strncpy(SYMBOL_TABLE[symbol_count].name, name, MAX_LABEL_LEN - 1);
    SYMBOL_TABLE[symbol_count].name[MAX_LABEL_LEN - 1] = '\0';
    SYMBOL_TABLE[symbol_count].address = address;
    symbol_count++;
}



Symbol* find_symbol(const char* name)
{
    for (int i = 0; i < symbol_count; ++i)
    {
        if (0 == strcmp(SYMBOL_TABLE[i].name, name))
        {
            return &SYMBOL_TABLE[i];
        }
    }
    return NULL;
}

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



char* clean_line(char* line) {
    // Remove comments
    char *hash_comment = strchr(line, '#');
    char *semicolon_comment = strchr(line, ';');
    char *comment_start = NULL;

    if (hash_comment && semicolon_comment) 
    {
        comment_start = (hash_comment < semicolon_comment) 
                        ? hash_comment : semicolon_comment;
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

    // Trim the leading whitespace
    char *p = line;
    while (*p && isspace(*p))
    {
        p++;
    }

    // Skip empty lines
    if ('\0' == *p)
    {
        return NULL;
    }
    return p;
}



// The main assembler logic.
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <assembly_file.asm>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input = fopen(argv[1], "r");
    if (!input)
    {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "** Assembling %s **\n", argv[1]);

    char line[512];
    char mnemonic[16];
    char operand_str[100];
    char label_name[MAX_LABEL_LEN];

    fprintf(stdout, "  Pass 1: Building symbol table...\n");

    word_t current_address = CODE_START;
    int data_mode = 0; // CODE = 0, DATA = 1

    while(fgets(line, sizeof(line), input))
    {
        char* p = clean_line(line);
        if (!p) { continue; } // Skip empty/comment lines

        // Check for labels
        char *label_end = strchr(p, ':');
        if (label_end)
        {
            size_t label_len = label_end - p;
            if (label_len >= MAX_LABEL_LEN)
            {
                fprintf(stderr, "Error: Label is too long at 0x%04X\n", current_address);
                status = EXIT_FAILURE;
                label_len = MAX_LABEL_LEN - 1;
            }
            strncpy(label_name, p, label_len);
            label_name[label_len] = '\0';
            
            // Add symbol to table
            add_symbol(label_name, current_address);

            p = label_end + 1;
            while (*p && isspace(*p)) { p++; }
            if ('\0' == *p) { continue; }
        }

        int fields = sscanf(p, "%15s %99s", mnemonic, operand_str);
        if (fields < 1) { continue; }

        if (0 == strcmp(mnemonic, ".DATA"))
        {
            data_mode = 1;
            if (0 == data_section_start_address)
            {
                data_section_start_address = current_address;
            }
            continue;
        }
        else if (0 == strcmp(mnemonic, ".CODE"))
        {
            data_mode = 0;
            continue;
        }

        // Increment address based on what we're parsing
        if (1 == data_mode)
        {
            if (0 == strcmp(mnemonic, ".WORD"))
            {
                current_address++;
            }
            else if (0 == strcmp(mnemonic, ".STRING"))
            {
                char *start_quote = strchr(p, '"');
                if (!start_quote) {
                    fprintf(stderr, "Error: .STRING missing opening quote.\n");
                    status = EXIT_FAILURE;
                    break;
                }
                start_quote++;
                char *end_quote = strchr(start_quote, '"');
                if (!end_quote) {
                    fprintf(stderr, "Error: .STRING missing closing quote.\n");
                    status = EXIT_FAILURE;
                    break;
                }
                *end_quote = '\0';
                
                // Calculate words used
                size_t len = strlen(start_quote);
                word_t words_used = 1 + (len + 1) / 2; // +1 for length, +1/2 for packed chars
                current_address += words_used;
            }
        } 
    else // CODE section
        {
            // All instructions are 1 word
            current_address++;
        }
    }

    // Check for errors from pass 1
    if (EXIT_FAILURE == status)
    {
        fprintf(stderr, "Errors found during Pass 1. Assembly halted.\n");
        fclose(input);
        return EXIT_FAILURE;
    }

    REGS.BR = data_section_start_address;
    if (0 == REGS.BR)
    {
        REGS.BR = current_address; // No .DATA section, so set BR to end of code
    }

    fprintf(stdout, "  Pass 2: Generating machine code...\n");

    rewind(input);
    current_address = CODE_START;
    data_mode = 0;
    word_t max_address_written = 0;
    MnemonicMap *map_entry = NULL;
    int operand = 0;

    while(fgets(line, sizeof(line), input))
    {
        char* p = clean_line(line);
        if (!p) { continue; }

        // Skip label definitions, we just care about code/data
        char *label_end = strchr(p, ':');
        if (label_end)
        {
            p = label_end + 1;
            while (*p && isspace(*p)) { p++; }
            if ('\0' == *p) { continue; }
        }

        int fields = sscanf(p, "%15s %99s", mnemonic, operand_str);
        if (fields < 1) { continue; }

        if (0 == strcmp(mnemonic, ".DATA"))
        {
            data_mode = 1;
            continue;
        }
        else if (0 == strcmp(mnemonic, ".CODE"))
        {
            data_mode = 0;
            continue;
        }

        // Handle DATA section
        if (1 == data_mode)
        {
            if (0 == strcmp(mnemonic, ".WORD"))
            {
                if (fields < 2)
                {
                    fprintf(stderr, "Error: .WORD missing value at address 0x%04X\n", current_address);
                    status = EXIT_FAILURE;
                    break;
                }
                operand = (int)strtol(operand_str, NULL, 0); // Handles decimal, 0x, etc.
                MEMORY[current_address++] = (word_t)operand;
            }
            else if (0 == strcmp(mnemonic, ".STRING"))
            {
                char *start_quote = strchr(p, '"');
                if (!start_quote) { /* error already caught in pass 1 */ break; }
                start_quote++;
                char *end_quote = strchr(start_quote, '"');
                if (!end_quote) { /* error already caught in pass 1 */ break; }
                *end_quote = '\0'; 
                
                word_t words_used = str_pack(start_quote, current_address);
                current_address += words_used;
            }
            else
            {
                fprintf(stderr, "Error: Unknown data directive: %s\n", mnemonic);
                status = EXIT_FAILURE;
                break;
            }
            
            if (current_address > max_address_written)
            {
                max_address_written = current_address;
            }
        } 
        else // Handle CODE section
        {
            word_t instruction_word = 0;
            map_entry = NULL;
            operand = 0; // Reset operand

            // Find instruction in map
            for (size_t i = 0; i < INSTRUCTION_COUNT; ++i) {
                if (0 == strcmp(mnemonic, INSTRUCTION_MAP[i].mnemonic)) {
                    map_entry = (MnemonicMap *)&INSTRUCTION_MAP[i];
                    break;
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
                // Resolve operand if required
                if (map_entry->requires_operand)
                {
                    if (fields < 2) {
                        fprintf(stderr, "Error: Instruction %s missing operand at address 0x%04X\n", mnemonic, current_address);
                        status = EXIT_FAILURE;
                        instruction_word = encode_format1(OP_ILLEGAL, 0); 
                    } 
                    else 
                    {
                        // Check if operand is a number or a label
                        char *endptr;
                        errno = 0;
                        operand = (int)strtol(operand_str, &endptr, 0);

                        if (errno == ERANGE || (*endptr != '\0' && !isspace(*endptr))) 
                        { 
                            // Not a simple number, try binary or label
                            if (0 == strncmp(operand_str, "0b", 2))
                            {
                                operand = (int)strtol(operand_str + 2, NULL, 2);
                            }
                            else // Must be a label then
                            {
                                Symbol* sym = find_symbol(operand_str);
                                if (!sym) 
                                {
                                    fprintf(stderr, "Error: Undefined label '%s' at address 0x%04X\n", operand_str, current_address);
                                    status = EXIT_FAILURE;
                                    instruction_word = encode_format1(OP_ILLEGAL, 0);
                                } 
                                else 
                                {
                                    if (OP_JMP == map_entry->opcode || OP_JAL == map_entry->opcode)
                                    {
                                        // PC-relative: operand = target_addr - (current_addr + 1)
                                        operand = (int)sym->address - (int)current_address - 1;
                                    }
                                    else if (OP_LDI == map_entry->opcode || OP_LOAD == map_entry->opcode || OP_STORE == map_entry->opcode)
                                    {
                                        // BR-relative: operand = target_addr - br_addr
                                        if (sym->address < REGS.BR) 
                                        {
                                            fprintf(stderr, "Warning: Data instruction '%s' references code label '%s' at 0x%04X. Using absolute address.\n", 
                                                mnemonic, operand_str, current_address);
                                            operand = (int)sym->address; // Fallback to absolute
                                        }
                                        else
                                        {
                                            operand = (int)sym->address - (int)REGS.BR;
                                        }
                                    }
                                    else
                                    {
                                        fprintf(stderr, "Error: Label '%s' used with non-data/non-jump instruction '%s' at 0x%04X\n", operand_str, mnemonic, current_address);
                                        status = EXIT_FAILURE;
                                        instruction_word = encode_format1(OP_ILLEGAL, 0);
                                    }
                                }
                            }
                        }
                        
                        if (0 == instruction_word)
                        {
                            // Warning for 12-bit truncation
                            if (operand > 4095 || operand < -2048)
                            {
                                word_t truncated_operand = ((word_t)operand) & 0x0FFF;
                                fprintf(stderr, "Warning: Operand value '%d' for instruction '%s' at address 0x%04X exceeds 12-bit range. It will be truncated to %u (0x%03X).\n",
                                        operand, mnemonic, current_address, truncated_operand, truncated_operand);
                            }
                            instruction_word = encode_format2(map_entry->opcode, operand);
                        }
                    }
                }
                else // Operand not required
                {
                    if (fields >= 2)
                    {
                        fprintf(stderr, "Warning: Operand '%s' for instruction '%s' at address 0x%04X will be ignored.\n",
                                operand_str, mnemonic, current_address);
                    }
                    
                    switch (map_entry->type) 
                    {
                        case TYPE_FORMAT1:
                            instruction_word = encode_format1(map_entry->opcode, map_entry->func_code);
                            break;
                        case TYPE_FORMAT2: // Should not happen
                        case TYPE_NO_FUNC:
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

    if (EXIT_FAILURE == status)
    {
        fprintf(stderr, "Errors found during Pass 2. Assembly halted.\n");
        fclose(input);
        return EXIT_FAILURE;
    }

    // This allows the simulator to know where the data section starts.
    MEMORY[0x0000] = REGS.BR; 

    word_t total_words_used = max_address_written;
    if (total_words_used < 1)
    {
        total_words_used = 1;
    }
    
    // Fallback if no .DATA section was ever defined
    if (total_words_used < current_address)
    {
        total_words_used = current_address;
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