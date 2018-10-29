#include <hal/i2c.h>

hal_err_t hal_i2c_bus_init(hal_i2c_bus_t bus, hal_gpio_t sda, hal_gpio_t scl, uint32_t freq_hz)
{
    esp_err_t err;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = freq_hz;
    if ((err = i2c_param_config(bus, &conf)) != ESP_OK)
    {
        return err;
    }
    // Buffers are only used in slave mode
    return i2c_driver_install(bus, conf.mode, 0, 0, 0);
}

hal_err_t hal_i2c_bus_deinit(hal_i2c_bus_t bus)
{
    return i2c_driver_delete(bus);
}

hal_err_t hal_i2c_cmd_init(hal_i2c_cmd_t *cmd)
{
    cmd->handle = i2c_cmd_link_create();
    return cmd->handle ? ESP_OK : ESP_ERR_NO_MEM;
}

hal_err_t hal_i2c_cmd_destroy(hal_i2c_cmd_t *cmd)
{
    i2c_cmd_link_delete(cmd->handle);
    cmd->handle = NULL;
    return ESP_OK;
}

hal_err_t hal_i2c_cmd_master_start(hal_i2c_cmd_t *cmd)
{
    return i2c_master_start(cmd->handle);
}

hal_err_t hal_i2c_cmd_master_stop(hal_i2c_cmd_t *cmd)
{
    return i2c_master_stop(cmd->handle);
}

hal_err_t hal_i2c_cmd_master_write_byte(hal_i2c_cmd_t *cmd, uint8_t data, bool ack_en)
{
    return i2c_master_write_byte(cmd->handle, data, ack_en);
}

hal_err_t hal_i2c_cmd_master_exec(hal_i2c_bus_t bus, hal_i2c_cmd_t *cmd)
{
    // TODO: Hardcoded timeout
    return i2c_master_cmd_begin(bus, cmd->handle, 10 / portTICK_PERIOD_MS);
}