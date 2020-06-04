#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

#if !defined(LOG_LEVEL_MINIMUM)
#define LOG_LEVEL_MINIMUM LOG_LEVEL_DEBUG
#endif

#define LOG_TAG_DECLARE(tag) static const char *TAG = tag;

#if LOG_LEVEL_MINIMUM <= LOG_LEVEL_DEBUG
#define LOG_D(tag, format, ...) log_printf(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define LOG_BUFFER_D(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_DEBUG, tag, buf, size)
#else
#define LOG_D(tag, format, ...)
#define LOG_BUFFER_D(tag, buf, size)
#endif

#if LOG_LEVEL_MINIMUM <= LOG_LEVEL_INFO
#define LOG_I(tag, format, ...) log_printf(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define LOG_BUFFER_I(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_INFO, tag, buf, size)
#else
#define LOG_I(tag, format, ...)
#define LOG_BUFFER_I(tag, buf, size)
#endif

#if LOG_LEVEL_MINIMUM <= LOG_LEVEL_WARNING
#define LOG_W(tag, format, ...) log_printf(LOG_LEVEL_WARNING, tag, format, ##__VA_ARGS__)
#define LOG_BUFFER_W(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_WARNING, tag, buf, size)
#else
#define LOG_W(tag, format, ...)
#define LOG_BUFFER_W(tag, buf, size)
#endif

#if LOG_LEVEL_MINIMUM <= LOG_LEVEL_ERROR
#define LOG_E(tag, format, ...) log_printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define LOG_BUFFER_E(tag, buf, size) log_print_buffer_hex(LOG_LEVEL_ERROR, tag, buf, size)
#else
#define LOG_E(tag, format, ...)
#define LOG_BUFFER_E(tag, buf, size)
#endif

#define LOG_F(tag, format, ...)                                  \
    do                                                           \
    {                                                            \
        log_printf(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__); \
        abort();                                                 \
    } while (0);

void log_printf(int level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
void log_print_buffer_hex(int level, const char *tag, const void *buffer, size_t size);
