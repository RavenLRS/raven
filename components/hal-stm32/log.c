#include <stdarg.h>
#include <stdio.h>

#include <hal/log.h>

void log_printf(log_level_e level, const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    printf("[%s] ", tag);
    vprintf(format, ap);
    printf("\n");

    va_end(ap);
}

void log_print_buffer_hex(log_level_e level, const char *tag, const void *buffer, size_t size)
{
    // TODO
}
