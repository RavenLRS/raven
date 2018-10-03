#include <driver/gpio.h>

void hal_init(void)
{
    // Enable ISR for GPIO interrupts (used for DIO lines in Semtech based radios)
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
}