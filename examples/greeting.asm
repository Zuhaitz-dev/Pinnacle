
.CODE
    ; Print MESSAGE. (SYS_WRITE)
    LDI 1   ; [0x0001]. STDOUT. Stack: [fd]
    LDI 1   ; [0x0002]. Stack: [fd, buf_addr]
    TRAP 1  ; [0x0003]. Stack: [length]
    DROP    ; [0x0004]. Stack: []

    ; Read user input. (SYS_READ)
    LDI 0   ; [0x0005]. STDIN. Stack: [fd]
    LDI 24  ; [0x0006]. Stack: [fd, buf_addr]
    LDI 20  ; [0x0007]. Stack: [fd, buf_addr, count]
    TRAP 0  ; [0x0008]. Stack: [bytes_read]
    STORE 0 ; [0x0009]. Stack: []
            ;           We are using the EXIT_CODE word to
            ;           save our temporal variable. 

    ; Print GREETING. (SYS_WRITE)
    LDI 1   ; [0x000A]. STDOUT. Stack: [fd]
    LDI 15  ; [0x000B]. Stack: [fd, buf_addr]
    TRAP 1  ; [0x000C]. Stack: [length]
    DROP    ; [0x000D]. Stack: []

    ; Print input. (SYS_WRITE)
    LDI 1   ; [0x000E]. STDOUT. Stack: [fd] 
    LDI 24  ; [0x000F]. Stack: [fd, buf_addr]
    TRAP 1  ; [0x0010]. Stack: [length]
    DROP    ; [0x0011]. Stack: []

    ; Finish the program. (SYS_EXIT)
    LDI 0   ; [0x0012]. Stack: [EXIT_CODE]
    STORE 0 ; [0x0013]. Stack: []
    TRAP 2  ; [0x0014]. END. Stack: []

.DATA
    EXIT_CODE: .WORD 0
    ; We could use a .RES word. Will this exist? Soon, I guess :p
    ; MESSAGE is 26 chars long (+ 1 word for offset).
    MESSAGE:  .STRING "Hello! What is your name? "
    ; GREETING is 18 chars long.
    GREETING: .STRING "Nice to meet you, "
    ; Starting on the 24th word, all space is not initialized.

    ; SLIGHT NOTE:  I believe in flexibility. There's no restriction at this point.
    ;               Write anything in the .DATA section. You can modify everything,
    ;               and you can write outside of the words loaded (after all, that 
    ;               just indicates the instructions + initialized data).
