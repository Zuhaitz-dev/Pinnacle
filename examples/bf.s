; Pinnacle Brainfuck Interpreter.
; Reads a Brainfuck program from stdin, stores it in memory, and executes it.

.CODE
; === PHASE 1: READ SCRIPT FROM STDIN ===
READ_LOOP:
    LDI 0           ; fd = 0 (stdin)
    LDI IO_BUF      ; buffer offset
    LDI 1           ; count = 1 byte
    TRAP 0          ; Read
    
    ; TRAP 0 pushes bytes_read onto the stack.
    ; If bytes_read <= 0, we've hit EOF.
    LDI 1           ; offset = 1 (to skip the JMP)
    BP              ; Branch Positive: If bytes_read > 0, skip the next instruction.
    JMP START_EXEC  ; Hit EOF! Jump to execution phase.

    ; Extract char from IO_BUF. TRAP 0 writes the char to the high byte.
    LOAD IO_BUF_VAL
    LDI 8
    SHR             ; Stack now holds: [char]
    
    ; Store char at (BF_CODE + CODE_LEN)
    LOAD CODE_LEN
    LDI BF_CODE
    ADD             ; Stack: [char, pointer]. (Pointer is on top)
    STOREI          ; NO SWAP NEEDED.
    
    ; Increment CODE_LEN
    LOAD CODE_LEN
    INC
    STORE CODE_LEN

    JMP READ_LOOP
    

; === PHASE 2: EXECUTION ===
START_EXEC:
    LDI 0
    STORE CODE_PTR
    
    ; Initialize the TAPE_PTR safely past the BF_CODE string.
    ; We are using LOAD to bypass the 12-bit signed immediate limit of LDI.
    ; Just a little quirk.
    LDI BF_CODE
    LOAD TAPE_OFFSET
    ADD
    STORE TAPE_PTR

EXEC_LOOP:
    ; Check if we have reached the end of the script
    LOAD CODE_PTR
    LOAD CODE_LEN
    SUB
    LDI 1
    BZ              ; If (CODE_PTR - CODE_LEN) == 0, skip the JMP.
    JMP FETCH_CHAR
    
    TRAP 2          ; Halt program successfully

FETCH_CHAR:
    LOAD CODE_PTR
    LDI BF_CODE
    ADD
    LOADI           ; Stack now holds: [char]

CHECK_PLUS:
    DUP
    LDI 43          ; '+'
    SUB
    LDI 1
    BZ              ; If val == 0 (it is '+'), skip JMP
    JMP CHECK_MINUS
    
    ; Execute '+'
    LOAD TAPE_PTR
    LOADI
    INC
    LOAD TAPE_PTR   ; Stack: [value+1, pointer]
    STOREI
    JMP NEXT_INST

CHECK_MINUS:
    DUP
    LDI 45          ; '-'
    SUB
    LDI 1
    BZ
    JMP CHECK_RIGHT
    
    ; Execute '-'
    LOAD TAPE_PTR
    LOADI
    DEC
    LOAD TAPE_PTR   ; Stack: [value-1, pointer]
    STOREI
    JMP NEXT_INST

CHECK_RIGHT:
    DUP
    LDI 62          ; '>'
    SUB
    LDI 1
    BZ
    JMP CHECK_LEFT
    
    ; Execute '>'
    LOAD TAPE_PTR
    INC
    STORE TAPE_PTR
    JMP NEXT_INST

CHECK_LEFT:
    DUP
    LDI 60          ; '<'
    SUB
    LDI 1
    BZ
    JMP CHECK_DOT
    
    ; Execute '<'
    LOAD TAPE_PTR
    DEC
    STORE TAPE_PTR
    JMP NEXT_INST

CHECK_DOT:
    DUP
    LDI 46          ; '.'
    SUB
    LDI 1
    BZ
    JMP CHECK_COMMA
    
    ; Execute '.'
    LOAD TAPE_PTR
    LOADI           ; Fetch value from tape
    LDI 8
    SHL             ; Shift to high byte for packed string output (another quirk of Pinnacle hehe)
    STORE IO_BUF_VAL

    LDI 1           ; Set string length to 1
    STORE IO_BUF    ; Restore the prefix overwritten by EOF
    
    LDI 1           ; fd = 1 (stdout)
    LDI IO_BUF
    TRAP 1          ; Print!
    DROP            ; Drop bytes_w

CHECK_COMMA:
    DUP
    LDI 44          ; ','
    SUB
    LDI 1
    BZ
    JMP CHECK_LBRACKET
    
    ; Execute ','
    LDI 0           ; fd = 0 (stdin)
    LDI IO_BUF
    LDI 1
    TRAP 0
    DROP            ; Drop bytes_read
    LOAD IO_BUF_VAL
    LDI 8
    SHR             ; Shift from high byte. Stack: [char]
    LOAD TAPE_PTR   ; Stack: [char, pointer]
    STOREI          ; Save to tape
    JMP NEXT_INST

CHECK_LBRACKET:
    DUP
    LDI 91          ; '['
    SUB
    LDI 1
    BZ
    JMP CHECK_RBRACKET
    
    ; Execute '[' (Jump forward if *TAPE_PTR == 0)
    LOAD TAPE_PTR
    LOADI
    LDI 1
    BZ              ; If val == 0, skip the next JMP to scan forward
    JMP NEXT_INST
    
LBRACKET_SKIP:
    LDI 1           ; Push depth = 1 onto stack
SCAN_FWD:
    LOAD CODE_PTR
    INC
    STORE CODE_PTR
    
    LOAD CODE_PTR
    LDI BF_CODE
    ADD
    LOADI           ; Stack: [depth, char]
    
    DUP
    LDI 91          ; '['
    SUB
    LDI 1
    BZ
    JMP FWD_NOT_L
    DROP            ; drop char
    INC             ; depth++
    JMP SCAN_FWD
    
FWD_NOT_L:
    DUP
    LDI 93          ; ']'
    SUB
    LDI 1
    BZ
    JMP FWD_NOT_R
    DROP            ; drop char
    DEC             ; depth--
    DUP
    LDI 1
    BZ              ; If depth == 0, skip the JMP
    JMP SCAN_FWD
    JMP SCAN_FWD_DONE
    
FWD_NOT_R:
    DROP            ; drop char
    JMP SCAN_FWD
    
SCAN_FWD_DONE:
    DROP            ; drop depth
    JMP NEXT_INST

CHECK_RBRACKET:
    DUP
    LDI 93          ; ']'
    SUB
    LDI 1
    BZ
    JMP IGNORE_CHAR
    
    ; Execute ']' (Jump backward if *TAPE_PTR != 0)
    LOAD TAPE_PTR
    LOADI
    LDI 1
    BNZ             ; If val != 0, skip the next JMP to scan backward
    JMP NEXT_INST
    
RBRACKET_LOOP:
    LDI 1           ; Push depth = 1 onto stack
SCAN_BWD:
    LOAD CODE_PTR
    DEC
    STORE CODE_PTR
    
    LOAD CODE_PTR
    LDI BF_CODE
    ADD
    LOADI           ; Stack: [depth, char]
    
    DUP
    LDI 93          ; ']'
    SUB
    LDI 1
    BZ
    JMP BWD_NOT_R
    DROP            ; drop char
    INC             ; depth++
    JMP SCAN_BWD
    
BWD_NOT_R:
    DUP
    LDI 91          ; '['
    SUB
    LDI 1
    BZ
    JMP BWD_NOT_L
    DROP            ; drop char
    DEC             ; depth--
    DUP
    LDI 1
    BZ              ; If depth == 0, skip the JMP
    JMP SCAN_BWD
    JMP SCAN_BWD_DONE
    
BWD_NOT_L:
    DROP            ; drop char
    JMP SCAN_BWD
    
SCAN_BWD_DONE:
    DROP            ; drop depth
    JMP NEXT_INST

IGNORE_CHAR:
    ; Do nothing, fall through to NEXT_INST
NEXT_INST:
    DROP            ; Drop the fetched `char` from the stack
    LOAD CODE_PTR
    INC
    STORE CODE_PTR
    JMP EXEC_LOOP

.DATA
    EXIT_CODE:   .WORD 0
    IO_BUF:      .WORD 1        ; Length prefix for string packing
    IO_BUF_VAL:  .WORD 0        ; High byte holds the character
    CODE_LEN:    .WORD 0
    CODE_PTR:    .WORD 0
    TAPE_PTR:    .WORD 0        ; Initialized at runtime
    TAPE_OFFSET: .WORD 2048     ; By-passes the LDI 12-bit limit
    BF_CODE:     .WORD 0        ; The BF Script array begins here
