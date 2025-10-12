
#include "isa_defs.h"
#include "string_utils.h"

#ifndef NO_LOG
#include <stdio.h>
__attribute__((weak)) FILE *log_file = NULL;
#endif



word_t str_pack(const char *str, word_t start_address)
{
    size_t len = strlen(str);
    word_t words_written = 0;

    // First, write the length of the string as the first word.
    MEMORY[start_address] = (word_t)len;
    words_written++;

    for (size_t i = 0; i < len; i += 2)
    {
        char c1 = str[i];
        char c2 = (i + 1 < len) ? str[i + 1] : 0;
        
        word_t packed_word = ((word_t)c1 << 8) | (word_t)c2;
        MEMORY[start_address + words_written] = packed_word;
        words_written++;
    }
    
    return words_written;
}



string_t str_unpack(word_t buf_offset)
{
    word_t addr = REGS.BR + buf_offset;
    word_t count = MEMORY[addr]; // Read the length prefix.
    
    char *tmp = malloc(count + 1);
    if (!tmp)
    {
        perror("malloc");
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }

    // Start reading from the word *after* the length prefix.
    for (word_t i = 0; i < count; i++)
    {
        char c = GET_CHAR_FROM_WORD(MEMORY[addr + 1 + i / 2], i);
        tmp[i] = c;
    }
    tmp[count] = '\0';

    string_t res = { .str = tmp, .count = count };
    return res;
}



void buf_pack(const char *buffer, word_t buf_offset, word_t count)
{
    word_t addr = REGS.BR + buf_offset;

    // First, write the number of bytes read as the length prefix.
    MEMORY[addr] = count;

    // Start writing the buffer content from the next word.
    for (word_t i = 0; i < count; i++)
    {
        word_t idx = (i / 2) + 1;
        if (0 == i % 2)
            MEMORY[addr + idx] = (buffer[i] & 0xFF) << 8;
        else
            MEMORY[addr + idx] |= (buffer[i] & 0xFF);
    }
}
