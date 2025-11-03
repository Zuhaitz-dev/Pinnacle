
; The binary should be 30 bytes big.

.CODE
    LDI 4         ; [0x0001] Stack: [C]. Push initial counter (4)

LOOP:
    ; Stack on entry: [C]
    DUP         ; [0x0002] Stack: [C, C]
    LDI 1       ; [0x0003] Stack: [C, C, 1]. Offset to JMP is 1 (PC + 1).
    
    ; BNZ will use C (value) and 1 (offset).
    BNZ         ; [0x0004] Stack: [C]
    
    ; If the counter is 0, we exit.
    JMP EXIT    ; [0x0005] Jump to TRAP 2.

EXIT_LOOP:
    DUP         ; [0x0006] Stack: [C, C]
    DEC         ; [0x0007] Stack: [C, C - 1]

    SWAP        ; [0x0008] Stack: [C - 1, C]
    LOAD RESULT ; [0x0009] Stack: [C - 1, C, F]
    
    MULT        ; [0x000A] Stack: [C - 1, R]
    STORE RESULT; [0x000B] Stack: [C - 1]
    
    JMP LOOP    ; [0x000C] Stack: [C - 1]. Jumps back to LOOP.

EXIT:
    TRAP 2      ; [0x000D]

.DATA
    ;We will use the space for the exit code 
    ; to show the result with `echo $?` 
    RESULT: .WORD 1  ; [0x000E]
