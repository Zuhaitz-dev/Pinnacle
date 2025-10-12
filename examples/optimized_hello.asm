
; The binary for this program would be 26 bytes big.

.CODE
    LDI 1   ; [0x0001]. Stack: [fd]
    LDI 0   ; [0x0002]. Stack: [fd, buf_addr]
    TRAP 1  ; [0x0003]. Stack: [length]

    HALT    ; [0x0004]. END. Stack: [length]

.DATA
    MESSAGE: .STRING "Hello, World!"    ; [0x0005].
