// platform_i2c_rp2040.c - RP2040 I2C implementation
//
// Wraps pico-sdk hardware/i2c.h for the platform I2C HAL.

#include "platform/platform_i2c.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define MAX_I2C_BUSES 2

// Static bus state (RP2040 has i2c0 and i2c1)
static struct platform_i2c {
    i2c_inst_t* inst;
    bool initialized;
} i2c_buses[MAX_I2C_BUSES];

platform_i2c_t platform_i2c_init(const platform_i2c_config_t* config)
{
    if (!config || config->bus >= MAX_I2C_BUSES) return NULL;

    struct platform_i2c* bus = &i2c_buses[config->bus];
    if (bus->initialized) return bus;

    bus->inst = (config->bus == 0) ? i2c0 : i2c1;

    i2c_init(bus->inst, config->freq_hz);
    gpio_set_function(config->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config->sda_pin);
    gpio_pull_up(config->scl_pin);

    bus->initialized = true;
    printf("[i2c:rp2040] Bus %d initialized (SDA=%d, SCL=%d, %luHz)\n",
           config->bus, config->sda_pin, config->scl_pin,
           (unsigned long)config->freq_hz);

    return bus;
}

int platform_i2c_write(platform_i2c_t bus, uint8_t addr, const uint8_t* data, size_t len)
{
    if (!bus || !bus->initialized) return -1;
    int ret = i2c_write_blocking(bus->inst, addr, data, len, false);
    return (ret == PICO_ERROR_GENERIC) ? -1 : 0;
}

int platform_i2c_read(platform_i2c_t bus, uint8_t addr, uint8_t* data, size_t len)
{
    if (!bus || !bus->initialized) return -1;
    int ret = i2c_read_blocking(bus->inst, addr, data, len, false);
    return (ret == PICO_ERROR_GENERIC) ? -1 : 0;
}

int platform_i2c_write_read(platform_i2c_t bus, uint8_t addr,
                            const uint8_t* wr, size_t wr_len,
                            uint8_t* rd, size_t rd_len)
{
    if (!bus || !bus->initialized) return -1;

    // Write with nostop=true for repeated start
    int ret = i2c_write_blocking(bus->inst, addr, wr, wr_len, true);
    if (ret == PICO_ERROR_GENERIC) return -1;

    ret = i2c_read_blocking(bus->inst, addr, rd, rd_len, false);
    return (ret == PICO_ERROR_GENERIC) ? -1 : 0;
}
