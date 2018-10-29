#pragma once

#include <stddef.h>
#include <stdio.h>

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
} log_level_e;

#define LOG_TAG_DECLARE(tag) static const char *TAG = tag;

#define LOG_D(tag, format, ...) log_printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define LOG_I(tag, format, ...) log_printf(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define LOG_W(tag, format, ...) log_printf(LOG_LEVEL_WARNING, tag, format, ##__VA_ARGS__)
#define LOG_E(tag, format, ...) log_printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define LOG_F(tag, format, ...)                                  \
    do                                                           \
    {                                                            \
        log_printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__); \
        abort();                                                 \
    } while (0);

#define LOG_BUFFER_D(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_DEBUG, tag, buf, size)
#define LOG_BUFFER_I(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_INFO, tag, buf, size)
#define LOG_BUFFER_W(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_WARNING, tag, buf, size)
#define LOG_BUFFER_E(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_ERROR, tag, buf, size)

void log_printf(log_level_e level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
void log_print_buffer_hex(log_level_e level, const char *tag, const void *buffer, size_t size);
