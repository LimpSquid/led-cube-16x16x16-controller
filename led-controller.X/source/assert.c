#include "../include/assert.h"
#include "../include/print.h"
#include <string.h>

#define ASSERT_BUFFER_SIZE  512

static inline void __attribute__((always_inline)) assert_printer(const char* format, va_list arg);
static inline unsigned char __attribute__((always_inline)) assert_halt(); 

static char assert_print_buffer[ASSERT_BUFFER_SIZE];

void __assert_print(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    assert_printer(format, arg);
    va_end(arg);
    assert_halt();
}

void __assert_print_no_block(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    assert_printer(format, arg);
    va_end(arg);
}

static inline void __attribute__((always_inline)) assert_printer(const char* format, va_list arg)
{
    memset(assert_print_buffer, 0, ASSERT_BUFFER_SIZE);
    print_vfs(assert_print_buffer, format, arg);
}

static inline unsigned char __attribute__((always_inline)) assert_halt()
{
    while(1) {
        Nop();
        Nop();
        Nop();
    }
}