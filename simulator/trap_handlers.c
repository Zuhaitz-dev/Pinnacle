
#include "trap_handlers.h"
#include <unistd.h>
#include <fcntl.h>



// Global TRAP Dispatch Table definition.
trap_handler_t TRAP_TABLE[256] = {0};

// Forward declarations for local trap handlers
void trap_read_handler(void);
void trap_write_handler(void);
void trap_exit_handler(void);
void trap_open_handler(void);
void trap_close_handler(void);



string_t unpack_string(word_t buf_offset, word_t count)
{
    word_t addr = REGS.BR + buf_offset;
    char *tmp = malloc(count + 1);
    if (!tmp)
    {
        perror("malloc");
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }

    word_t actual_len = count;
    for (word_t i = 0; i < count; i++)
    {
        char c = GET_CHAR_FROM_WORD(MEMORY[addr + i / 2], i);
        tmp[i] = c;
        if ('\0' == c)
        {
            actual_len = i;
            break;
        }
    }
    tmp[actual_len] = '\0';

    string_t res = { .str = tmp, .count = actual_len };
    return res;
}



// Syscall 0: read(fd, buf_offset, count);
void trap_read_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // Okay, this is pretty much like SYS_WRITE in the sense that
    // the arguments are pretty much the same.
    // (Yes, even if this appears first, I think, I implemented SYS_WRITE first).
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    if (count > MAX_STACK_READ_SIZE)
    {
        fprintf(stderr, "Runtime Error: Read request of %u bytes exceeds maximum of %d\n", count, MAX_STACK_READ_SIZE);
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }

    char tmp[count];
    ssize_t n = read((int)fd, tmp, count);

    if (n <= 0)
    {
        MEMORY[--REGS.SP] = (word_t)n;
        return;
    }

    word_t addr = REGS.BR + buf_offset;
    for (ssize_t i = 0; i < n; i++)
    {
        word_t idx = i / 2;
        if (0 == i % 2)
            MEMORY[addr + idx] = (tmp[i] & 0xFF) << 8; 
        else
            MEMORY[addr + idx] |= (tmp[i] & 0xFF);
    }

    MEMORY[--REGS.SP] = (word_t)n;
}



// Syscall 1: write(fd, buf_offset, count);
void trap_write_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // The parameters are popped from the stack in the reverse order they were pushed.
    // POP Count (length).
    word_t count = MEMORY[REGS.SP++];
    // POP Buffer Offset (which is relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    string_t write_str = unpack_string(buf_offset, count);
    write((1 == fd ? STDOUT_FILENO : (2 == fd ? STDERR_FILENO : fd)), write_str.str, write_str.count);
    free(write_str.str);
    MEMORY[--REGS.SP] = write_str.count;
}



// Syscall 2: exit(status);
void trap_exit_handler()
{
    // The exitcode is stored at the Base Register.
    status = MEMORY[REGS.BR];
}



// Syscall 3: open(*path, flags, count);
// Count is added as a way to actually unpack this.
// There are other options, like mode, but let's work with this.
void trap_open_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // POP count. If not, good luck.
    word_t count = MEMORY[REGS.SP++];
    // POP Flags. Check: https://man7.org/linux/man-pages/man2/open.2.html
    word_t flags = MEMORY[REGS.SP++];
    // POP path. We still have to process it.
    word_t buf_offset = MEMORY[REGS.SP++];

    string_t open_str = unpack_string(buf_offset, count);
    int fd = open(open_str.str, (int)flags);

    if (fd == -1)
    {
        perror("open");
        free(open_str.str);
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }

    free(open_str.str);
    MEMORY[--REGS.SP] = (word_t)fd;
}



// Syscall 4: close(fd);
void trap_close_handler()
{
    // POP fd.
    word_t fd = MEMORY[REGS.SP++];

    // (https://man7.org/linux/man-pages/man2/close.2.html)
    // close() returns zero on success.  On error, -1 is returned, and
    // errno is set to indicate the error.
    if (close(fd) != 0)
    {
        fprintf(stderr, "Could not close the file.\n");
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }
}



// We have 256 options, could we expand this later if so? Yeah?
void initialize_trap_table()
{
    // For now we have this one.
    // TODO: Add the implementation for more system calls.
    TRAP_TABLE[0] = trap_read_handler;
    TRAP_TABLE[1] = trap_write_handler;
    TRAP_TABLE[2] = trap_exit_handler;
    TRAP_TABLE[3] = trap_open_handler;
    TRAP_TABLE[4] = trap_close_handler;
}
