#include "i2c_bus.h"
#include "band_config.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

esp_err_t i2c_bus_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t i2c_write_reg(uint8_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(I2C_PORT, dev, buf, sizeof(buf),
                                      pdMS_TO_TICKS(100));
}

esp_err_t i2c_read_regs(uint8_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(I2C_PORT, dev, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(100));
}
