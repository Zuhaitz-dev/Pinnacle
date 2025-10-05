
#ifndef ASSEMBLER_DEFS_H
#define ASSEMBLER_DEFS_H

typedef enum
{
    TYPE_FORMAT1,   // Uses encode_format1 with FUNC_ code.
    TYPE_FORMAT2,   // Uses encode_format2 with operand.
    TYPE_NO_FUNC    // Uses encode_format1 with 0 func.
} InstructionType;

typedef struct
{
    const char *mnemonic;
    InstructionType type;
    int opcode;             // For all instructions.
    int func_code;          // For TYPE_ALU_LOGIC, TYPE_SHIFT.
    int requires_operand;   // 1 if fields < 2 is an error, 0 otherwise.
} MnemonicMap;

#endif
