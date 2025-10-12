
#include "trap_handlers.h"
#include "string_utils.h"
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



// Syscall 0: read(fd, buf_offset, count);
void trap_read_handler()
{
    CHECK_SP_UNDERFLOW(3);
    // POP Max Count to read.
    word_t max_count = MEMORY[REGS.SP++];
    // POP Buffer Offset (relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    if (max_count > MAX_STACK_READ_SIZE)
    {
        fprintf(stderr, "Runtime Error: Read request of %u bytes exceeds maximum of %d\n", max_count, MAX_STACK_READ_SIZE);
        CLOSE_LOG();
        exit(EXIT_FAILURE);
    }

    char tmp[max_count];
    ssize_t n = read((int)fd, tmp, max_count);

    if (n < 0)
    {
        MEMORY[--REGS.SP] = (word_t)n;
        return;
    }

    // Pack the buffer into memory, including the length prefix.
    buf_pack(tmp, buf_offset, n);

    MEMORY[--REGS.SP] = (word_t)n;
}



// Syscall 1: write(fd, buf_offset, count);
void trap_write_handler()
{
    CHECK_SP_UNDERFLOW(2);
    // POP Buffer Offset (relative to BR).
    word_t buf_offset = MEMORY[REGS.SP++];
    // POP File Descriptor.
    word_t fd = MEMORY[REGS.SP++];

    string_t write_str = str_unpack(buf_offset);
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
    CHECK_SP_UNDERFLOW(2);
    // POP Flags. Check: https://man7.org/linux/man-pages/man2/open.2.html
    word_t flags = MEMORY[REGS.SP++];
    // POP path. We still have to process it.
    word_t buf_offset = MEMORY[REGS.SP++];

    string_t open_str = str_unpack(buf_offset);
    int fd = open(open_str.str, (int)flags);

    if (-1 == fd)
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
