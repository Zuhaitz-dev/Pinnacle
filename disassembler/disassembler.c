
#include "isa_defs.h"
#include <ctype.h>



// Helper function.
void ASCII_representation(const unsigned char high_char, const unsigned char low_char)
{
    if (isprint((unsigned char)high_char) || isprint((unsigned char)low_char))
    {
        printf("\t; ");
        if (isprint((unsigned char)high_char))
        {
            printf("'%c'", high_char);
        }
        else
        {
            printf("  "); // To keep the alignment.
        }
        if (isprint((unsigned char)low_char))
        {
            printf(" '%c'", low_char);
        }
    }
}

const char* alu_map[] = {
    [FUNC_ADD] = "ADD", [FUNC_SUB] = "SUB", [FUNC_MULT] = "MULT",
    [FUNC_DIV] = "DIV", [FUNC_NEG] = "NEG", [FUNC_INC]  = "INC",
    [FUNC_DEC] = "DEC", [FUNC_ABS] = "ABS", [FUNC_NOT]  = "NOT",
    [FUNC_AND] = "AND", [FUNC_OR]  = "OR",  [FUNC_XOR]  = "XOR",
    [FUNC_SHL] = "SHL", [FUNC_SHR] = "SHR"
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
    int simple_mode = 0;
    char *filename = NULL;

    if (2 == argc)
    {
        filename = argv[1];
    }
    else if (3 == argc && 0 == strcmp(argv[1], "-s"))
    {
        simple_mode = 1;
        filename = argv[2];
    }
    else
    {
        fprintf(stderr, "Usage: %s [-s] <binary_file.bin>\n", argv[0]);
        fprintf(stderr, "  -s: Simple mode for re-assemblable output\n");
        return EXIT_FAILURE;
    }


    FILE *input = fopen(filename, "rb");
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
        
        if (!simple_mode)
        {
            printf("\t[%#06X] ", pc);
        }
        else
        {
            printf("\t");
        }

        switch (__builtin_expect(opcode, OP_LDI))
        {
            case OP_ALU_LOGIC:
                printf("%s", alu_map[arg]);
                break;
            case OP_STACK_OPS:
                printf("%s", stack_op_map[arg]);
                break;
            case OP_BRANCH:
                printf("%s", branch_map[arg]);
                break;
            case OP_LDI:
                printf("LDI %d", sign_extend_12(arg));
                break;
            case OP_LOAD:
                printf("LOAD %d", sign_extend_12(arg));
                break;
            case OP_STORE:
                printf("STORE %d", sign_extend_12(arg));
                break;
            case OP_JMP:
                printf("JMP %d", sign_extend_12(arg));
                break;
            case OP_JAL:
                printf("JAL %d", sign_extend_12(arg));
                break;
            case OP_TRAP:
                printf("TRAP %d", arg);
                break;
            case OP_RET:
                printf("RET");
                break;
            case OP_HALT:
                printf("HALT");
                break;
            case OP_ILLEGAL:
            default:
                printf("ILLEGAL");
                break;
        }
        if (simple_mode)
        {
            printf("\n");
        }
        else
        {
            printf("\t(%#06X)\n", instr.raw);
        }
    }

    if (words_read > data_start_address)
    {
        printf("\n.DATA\n");
        
        for (size_t i = data_start_address; i < words_read; ++i)
        {
            word_t current_word = MEMORY[i];
            if (simple_mode)
            {
                printf("\t.WORD %-6d", current_word);
            }
            else
            {
                printf("\t[%#06zX] .WORD %-6d (%#06X)", i, current_word, current_word);
            }

            const unsigned char high_char = GET_CHAR_FROM_WORD(current_word, 0);
            const unsigned char low_char = GET_CHAR_FROM_WORD(current_word, 1);
            
            ASCII_representation(high_char, low_char);

            printf("\n");
        }    
    }

    return EXIT_SUCCESS;
}