// seesaw.c - Adafruit Seesaw I2C Protocol Driver
//
// Implements the seesaw protocol: write [module][function], delay, read result.

#include "seesaw.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

void seesaw_init(seesaw_device_t* dev, platform_i2c_t bus, uint8_t addr)
{
    dev->bus = bus;
    dev->addr = addr;
}

bool seesaw_reset(seesaw_device_t* dev)
{
    uint8_t cmd[] = { SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF };
    int ret = platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd));
    if (ret == 0) {
        platform_sleep_ms(10);  // Seesaw needs time to reboot
    }
    return ret == 0;
}

uint8_t seesaw_get_hw_id(seesaw_device_t* dev)
{
    uint8_t cmd[] = { SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID };
    uint8_t id = 0;

    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return 0;
    platform_sleep_us(1000);
    if (platform_i2c_read(dev->bus, dev->addr, &id, 1) != 0) return 0;

    return id;
}

bool seesaw_gpio_set_input_pullup(seesaw_device_t* dev, uint32_t pin_mask)
{
    // Set direction to input (DIRCLR)
    uint8_t cmd[6] = {
        SEESAW_GPIO_BASE, SEESAW_GPIO_DIRCLR,
        (pin_mask >> 24) & 0xFF,
        (pin_mask >> 16) & 0xFF,
        (pin_mask >> 8) & 0xFF,
        pin_mask & 0xFF
    };
    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return false;
    platform_sleep_us(1000);

    // Enable pull-ups (PULLENSET)
    cmd[1] = SEESAW_GPIO_PULLENSET;
    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return false;
    platform_sleep_us(1000);

    // Set pins high to activate pull-ups (BULK_SET)
    cmd[1] = SEESAW_GPIO_BULK_SET;
    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return false;
    platform_sleep_us(1000);

    return true;
}

uint32_t seesaw_gpio_read_bulk(seesaw_device_t* dev)
{
    uint8_t cmd[] = { SEESAW_GPIO_BASE, SEESAW_GPIO_BULK };
    uint8_t buf[4] = {0};

    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return 0xFFFFFFFF;
    platform_sleep_us(1000);
    if (platform_i2c_read(dev->bus, dev->addr, buf, 4) != 0) return 0xFFFFFFFF;

    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

uint16_t seesaw_adc_read(seesaw_device_t* dev, uint8_t channel)
{
    uint8_t cmd[] = { SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + channel };
    uint8_t buf[2] = {0};

    if (platform_i2c_write(dev->bus, dev->addr, cmd, sizeof(cmd)) != 0) return SEESAW_ADC_ERROR;
    platform_sleep_us(1000);
    if (platform_i2c_read(dev->bus, dev->addr, buf, 2) != 0) return SEESAW_ADC_ERROR;

    return ((uint16_t)buf[0] << 8) | buf[1];
}
