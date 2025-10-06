
; The binary should be 32 bytes big.

.CODE
    ; First number can be changed, it indicates n, such as:
    ;   fibonacci(n) : n + fibonnaci(n - 1)
    ;   fibonacci(2) : 1
    ;   fibonacci(1) : 0

    ; Few notes,

    ; To use the less amount of instructions as possible to reduce
    ; the byte-size, we are not using two DEC operations, which would
    ; be 4 extra-bytes. The only issue is that n as in fibonacci(n)
    ; actually is n + 2.

    ; Therefore, with LDI 10, you would get 89 (which is the 12th element).

    LDI 10  ; [0x0001]. Stack: [n]
    DUP     ; [0x0002]. Stack: [n, n]
    LDI 1   ; [0x0003]. Stack: [n, n, offset]
    BNZ     ; [0x0004]. Stack: [n]

EXIT_LOOP:
    ; Finish the program. (SYS_EXIT)
    ; The n-th Fibonacci element.
    TRAP 2  ; [0x0005]. END. Stack: [n]

LOOP:
    LOAD 0  ; [0x0006]. Stack: [n, first]
    DUP     ; [0x0007]. Stack: [n, first, first]
    LOAD 1  ; [0x0008]. Stack: [n, first, first, second]
    ADD     ; [0x0009]. Stack: [n, first, sum]
    STORE 0 ; [0x000A]. Stack: [n, first]
    STORE 1 ; [0x000B]. Stack: [n]
    DEC     ; [0x000C]. Stack: [n - 1]
    JMP -12 ; [0x000D]. Stack: [n - 1]

.DATA
    FIRST:  .WORD 1
    SECOND: .WORD 0
