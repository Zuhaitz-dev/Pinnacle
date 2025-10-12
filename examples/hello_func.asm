
.CODE
    ; JAL expects a relative offset to the target function.
    ; Offset calculation: (Target - Current PC)
    JAL 1   ; [0x0001]. Stack: [LR]

    TRAP 2  ; [0x0002]. SYS_EXIT. Stack: []

    ; PUSH parameters for TRAP 1 (write)
    ; Stack format: [..., fd, buf_addr, count]

    ; PUSH File Descriptor (stdout = 1)
    LDI 1   ; [0x0003]. Stack: [LR, fd]

    ; PUSH Buffer Address offset.
    ; (MESSAGE is at offset 1 relative to BR)
    LDI 1   ; [0x0004]. Stack: [LR, fd, buf_addr]

    ; Execute the syscall
    TRAP 1  ; [0x0006]. Stack: [LR]

    ; Return to the caller (main code)
    RET     ; [0x0007]. Stack: [LR]

.DATA

    EXIT_CODE:  .WORD 42
        ; Offset 0 (Used by TRAP 2/EXIT).
        ; You can write here, but... It's reserved.
    MESSAGE:    .STRING "Hello, everyone!"
        ; Message is 16 chars long.
