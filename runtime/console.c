// Bridge DOOM's diagnostics to the Loopy serial port via Picolibc.
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "i_system.h"

extern void loopy_log_putc(char ch);

static int serial_put(char ch, FILE *stream)
{
    (void)stream;
    loopy_log_putc(ch);
    return (unsigned char)ch;
}

static FILE serial_stream =
    FDEV_SETUP_STREAM(serial_put, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdout = &serial_stream;
FILE *const stderr = &serial_stream;

/*
 * The cartridge uses the Renesas calling convention. Picolibc uses the
 * GNU calling convention, which passes variadic arguments differently.
 * On SH-1 both conventions use a pointer for va_list.
 */
int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int sprintf(char *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsprintf(buffer, format, args);
    va_end(args);
    return result;
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

/* Picolibc calls this for aborts. A cartridge has no host to return to. */
_Noreturn void _exit(int status)
{
    I_Error("C library terminated with status %d", status);
    __builtin_unreachable();
}
