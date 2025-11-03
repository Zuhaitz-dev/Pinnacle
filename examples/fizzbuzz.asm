
.CODE
    START:
        LOAD EXIT_CODE
        LDI 3
        DIV
        DROP
        LDI 1
        BNZ 
        JMP FIZZ 
    CALC_BUZZ:
        LOAD EXIT_CODE
        LDI 5
        DIV 
        DROP
        LDI 1
        BNZ 
        JMP BUZZ
    EXIT:
        TRAP 2
    FIZZ:
        LDI 1
        LDI 1
        LDI FIZZ_STR 
        TRAP 1
        JMP CALC_BUZZ
    BUZZ:
        LDI 1
        LDI 1
        LDI BUZZ_STR
        TRAP 1
        JMP EXIT

.DATA
    EXIT_CODE: .WORD 15         ; We will use the exit_code space for the number.
    FIZZ_STR: .STRING "Fizz"
    BUZZ_STR: .STRING "Buzz"
