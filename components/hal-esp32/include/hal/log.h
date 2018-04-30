#pragma once

#include <esp_log.h>

#define LOG_D(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#define LOG_I(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define LOG_W(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define LOG_E(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)

#define LOG_BUFFER_D(tag, buf, size) ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, size, ESP_LOG_DEBUG)
#define LOG_BUFFER_I(tag, buf, size) ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, size, ESP_LOG_INFO)
#define LOG_BUFFER_W(tag, buf, size) ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, size, ESP_LOG_WARN)
#define LOG_BUFFER_E(tag, buf, size) ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, size, ESP_LOG_ERROR)
