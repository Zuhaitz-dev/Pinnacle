
.CODE
    ; PUSH parameters for TRAP 1 (write)
    ; Parameters are read from the stack: fd, buf_addr.
    ; Count is included in the string.

    ; We PUSH the file descriptor (stdout = 1)
    LDI 1   ; [0x0001]. Stack: [fd]
    ; Then we PUSH the buffer address (MESSAGE starts at BR + 1)
    ; BR can be used but it's reserved for the Exit Code Word, used by TRAP 2.
    LDI 1   ; [0x0002]. Stack: [fd, buf_addr]

    ; Time to execute the SYS_WRITE syscall.
    ; TRAP 1 is mapped to 'write' in the simulator, but we can change this.
    TRAP 1  ; [0x0003]. SYS_WRITE. Stack: [length]

    DROP    ; [0x0004]. Stack: []

    TRAP 2  ; [0x0005]. SYS_EXIT. Stack: []

    ; Now that we have SYS_EXIT, we don't really need halt.
    ; I just leave it here to show another alternative.
    ; HALT

.DATA
    EXIT_CODE: .WORD 10                 ; [0x0006]. 
    MESSAGE: .STRING "Hello, world!"    ; [0x0007].
