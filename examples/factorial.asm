
; The binary should be 30 bytes big.

.CODE
    LDI 4         ; [0x0001] Push initial counter (4)

LOOP:
    ; Stack on entry: [C]
    DUP         ; [0x0002] [C, C]
    LDI 1       ; [0x0003] [C, C, 1]. Offset to JMP is 1 (PC + 1).
    
    ; BNZ will use C (value) and 1 (offset).
    BNZ         ; [0x0004] Branches 1 word forward (to 0x0006, remember PC is already pointing to 0x0005) if C != 0. Pops 1 and C. Stack: [C].
    
    ; If the counter is 0, we exit.
    JMP 7       ; [0x0005] Jump to TRAP 2 (0x0008).

EXIT_LOOP:
    DUP         ; [0x0006] Stack: [C, C]
    DEC         ; [0x0007] Stack: [C, C - 1]

    SWAP        ; [0x0008] Stack: [C - 1, C]
    LOAD 0      ; [0x0009] Stack: [C - 1, C, F]
    
    MULT        ; [0x000A] Stack: [C - 1, R]
    STORE 0
    
    JMP -11     ; [0x000B] Loop back to LOOP: at 0x0002. Remember, the PC is pointing to the next instruction
                ;          Therefore, your current instruction, 0x000B, is not 0, but -1.

    TRAP 2      ; [0x000C]

.DATA
    ;We will use the EXIT_CODE to show the result with echo %? 
    EXIT_CODE: .WORD 1  ; [0x000D]
