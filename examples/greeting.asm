
.CODE
    ; Print MESSAGE. (SYS_WRITE)
    LDI 1   ; [0x0001]. STDOUT. Stack: [fd]
    LDI 1   ; [0x0002]. Stack: [fd, buf_addr]
    LDI 26  ; [0x0003]. Stack: [fd, buf_addr, count]
    TRAP 1  ; [0x0004]. Stack: [length]
    DROP    ; [0x0005]. Stack: []

    ; Read user input. (SYS_READ)
    LDI 0   ; [0x0006]. STDIN. Stack: [fd]
    LDI 24  ; [0x0007]. Stack: [fd, buf_addr]
    LDI 20  ; [0x0008]. Stack: [fd, buf_addr, count]
    TRAP 0  ; [0x0009]. Stack: [bytes_read]
    STORE 0 ; [0x000A]. Stack: []
            ;           We are using the EXIT_CODE word to
            ;           save our temporal variable. 

    ; Print GREETING. (SYS_WRITE)
    LDI 1   ; [0x000B]. STDOUT. Stack: [fd]
    LDI 15  ; [0x000C]. Stack: [fd, buf_addr]
    LDI 18  ; [0x000D]. Stack: [fd, buf_addr, count]
    TRAP 1  ; [0x000E]. Stack: [length]
    DROP    ; [0x000F]. Stack: []

    ; Print input. (SYS_WRITE)
    LDI 1   ; [0x0010]. STDOUT. Stack: [fd] 
    LDI 24  ; [0x0011]. Stack: [fd, buf_addr]
    LOAD 0  ; [0x0012]. Stack: [fd, buf_addr, bytes_read]
    TRAP 1  ; [0x0013]. Stack: [length]
    DROP    ; [0x0014]. Stack: []

    ; Finish the program. (SYS_EXIT)
    LDI 0   ; [0x0015]. Stack: [EXIT_CODE]
    STORE 0 ; [0x0016]. Stack: []
    TRAP 2  ; [0x0017]. END. Stack: []

.DATA
    EXIT_CODE: .WORD 0
    ; We could use a .RES word. Will this exist? Soon, I guess :p
    ; MESSAGE is 26 chars long (+ 1 word for offset).
    ; So, how many words? The rule of thumb:
    ;   space_required(n) : n / 2 + (n % 2)
    ; Therefore: 26 / 2 + (26 % 2) = 13 + 0 = 13 words required.
    MESSAGE:  .STRING "Hello! What is your name? "
    ; GREETING is 18 chars long.
    ; It starts in the 15th word (14 already used).
    ;   space_required(18) : 18 / 2 + (18 % 2) = 9 words required.
    GREETING: .STRING "Nice to meet you, "
    ; Starting on the 24th word, all space is not initialized.

    ; SLIGHT NOTE:  I believe in flexibility. There's no restriction at this point.
    ;               Write anything in the .DATA section. You can modify everything,
    ;               and you can write outside of the words loaded (after all, that 
    ;               just indicates the instructions + initialized data).
