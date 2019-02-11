#include <stdarg.h>
#include <stdio.h>

#include <hal/log.h>

static const char log_levels[] = {
    'D',
    'I',
    'W',
    'E',
};

void log_printf(int level, const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    printf("[%s - %c] ", tag, log_levels[level]);
    vprintf(format, ap);
    printf("\n");

    va_end(ap);
}

void log_print_buffer_hex(int level, const char *tag, const void *buffer, size_t size)
{
    // Print lines of up to per_line bytes per line.
    // We need 5 characters per byte: 0xAB[space|\n]
    const size_t per_line = 8;
    const uint8_t *input = buffer;

    printf("[%s - %c] ", tag, log_levels[level]);

    for (size_t ii = 0; ii < size; ii++)
    {
        printf("0x%02x", input[ii]);
        if ((ii > 0 && ii % per_line == 0) || ii == size - 1)
        {
            printf("\n");
            printf("[%s - %c] ", tag, log_levels[level]);
        }
        else
        {
            printf(" ");
        }
    }
}
