
; The binary for this program would be 28 bytes big.

.CODE
    LDI 1   ; [0x0001]. Stack: [fd]
    LDI 0   ; [0x0002]. Stack: [fd, buf_addr]
    LDI 13  ; [0x0003]. Stack: [fd, buf_addr, count]
    TRAP 1  ; [0x0004]. Stack: []

    HALT    ; [0x0005]. END. Stack: []

.DATA
    MESSAGE: .STRING "Hello, World!"    ; [0x0006].
