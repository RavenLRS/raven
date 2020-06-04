#include <stdarg.h>
#include <stdio.h>

#include <hal/log.h>

static const char log_levels[] = {
    'D',
    'I',
    'W',
    'E',
};

static void log_print_prefix(int level, const char *tag)
{
    extern uint32_t xTaskGetTickCount(void);
    printf("[%u:%s - %c] ", (unsigned)xTaskGetTickCount(), tag, log_levels[level]);
}

void log_printf(int level, const char *tag, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    log_print_prefix(level, tag);
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

    log_print_prefix(level, tag);

    for (size_t ii = 0; ii < size; ii++)
    {
        printf("0x%02x", input[ii]);
        if ((ii > 0 && ii % per_line == 0) || ii == size - 1)
        {
            printf("\n");
            if (ii < size - 1)
            {
                log_print_prefix(level, tag);
            }
        }
        else
        {
            printf(" ");
        }
    }
}
