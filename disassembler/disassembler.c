
#include "isa_defs.h"
#include <ctype.h>



const char* alu_arith_map[] = {
    [FUNC_ADD] = "ADD", [FUNC_SUB] = "SUB", [FUNC_MULT] = "MULT",
    [FUNC_DIV] = "DIV", [FUNC_NEG] = "NEG", [FUNC_INC]  = "INC",
    [FUNC_DEC] = "DEC", [FUNC_ABS] = "ABS"
};

const char* alu_logic_map[] = {
    [FUNC_NOT - 0x800] = "NOT", [FUNC_AND - 0x800] = "AND",
    [FUNC_OR  - 0x800] = "OR",  [FUNC_XOR - 0x800] = "XOR",
    [FUNC_SHL - 0x800] = "SHL", [FUNC_SHR - 0x800] = "SHR"
};

const char* stack_op_map[] = {
    [FUNC_SWAP] = "SWAP", [FUNC_DUP]  = "DUP",
    [FUNC_DROP] = "DROP", [FUNC_OVER] = "OVER"
};

const char* branch_map[] = {
    [FUNC_BEQ] = "BEQ", [FUNC_BNE] = "BNE", [FUNC_BZ] = "BZ",
    [FUNC_BNZ] = "BNZ", [FUNC_BN] = "BN",   [FUNC_BP] = "BP"
};



int main(int argc, char **argv)
{
    if (2 != argc)
    {
        fprintf(stderr, "Usage: %s <binary_file.bin>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input = fopen(argv[1], "rb");
    if (!input)
    {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    word_t MEMORY[MEMORY_SIZE] = {0};
    size_t words_read = fread(MEMORY, sizeof(word_t), MEMORY_SIZE, input);
    fclose(input);

    if (0 == words_read)
    {
        fprintf(stderr, "Warning: Binary file is empty.\n");
        return EXIT_SUCCESS;
    }

    word_t data_start_address = MEMORY[0x0000];
    if (0 == data_start_address)
    {
        // If BR is 0, that means there's no .DATA section.
        // Therefore we will treat the whole file as code.
        data_start_address = words_read;
    }

    printf(".CODE\n");
    for (word_t pc = CODE_START; pc < data_start_address; ++pc)
    {
        Instruction instr;
        instr.raw = MEMORY[pc];
        uint16_t opcode = instr.fields.opcode;
        uint16_t arg    = instr.fields.arg;
        
        printf("    [%#06X] ", pc);
    
        switch (opcode)
        {
            case OP_ALU_LOGIC:
                if (arg >= FUNC_NOT)
                    printf("%s\n", alu_logic_map[arg - 0x800]);
                    else
                    printf("%s\n", alu_arith_map[arg]);
                break;
            case OP_STACK_OPS:
                printf("%s\n", stack_op_map[arg]);
                break;
            case OP_BRANCH:
                printf("%s\n", branch_map[arg]);
                break;
            case OP_LDI:
                printf("LDI %d\n", sign_extend_12(arg));
                break;
            case OP_LOAD:
                printf("LOAD %d\n", sign_extend_12(arg));
                break;
            case OP_STORE:
                printf("STORE %d\n", sign_extend_12(arg));
                break;
            case OP_JMP:
                printf("JMP %d\n",sign_extend_12(arg));
                break;
            case OP_JAL:
                printf("JAL %d\n", sign_extend_12(arg));
                break;
            case OP_TRAP:
                printf("TRAP %d\n", arg);
                break;
            case OP_RET:
                printf("RET\n");
                break;
            case OP_HALT:
                printf("HALT\n");
                break;
            case OP_ILLEGAL:
            default:
                printf("ILLEGAL (%#04X)\n", instr.raw);
                break;
        }
    }

    if (words_read > data_start_address)
    {
        printf("\n.DATA\n");
        for (size_t i = data_start_address; i < words_read; ++i)
        {
            word_t current_word = MEMORY[i];
            char high_char = GET_CHAR_FROM_WORD(current_word, 0);
            char low_char = GET_CHAR_FROM_WORD(current_word, 1);

            printf("    [%#06zX] .WORD %-6d (%#06X)", i, current_word, current_word);
            
            if (isprint((unsigned char)high_char) || isprint((unsigned char)low_char))
            {
                printf("\t; ");
                if (isprint((unsigned char)high_char))
                    printf("'%c'", high_char);
                else
                    printf("  "); // To keep the alignment.

                if (isprint((unsigned char)low_char))
                    printf(" '%c'", low_char);
            }
            printf("\n");
        }    
    }

    return EXIT_SUCCESS;
}