// display_spi.c - SPI transport for SH1106 OLED
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// SPI transport extracted from display.c for MacroPad RP2040.

#include "display.h"
#include "display_transport.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static spi_inst_t* spi = NULL;
static uint8_t pin_cs = 0;
static uint8_t pin_dc = 0;

static inline void cs_select(void) {
    gpio_put(pin_cs, 0);
}

static inline void cs_deselect(void) {
    gpio_put(pin_cs, 1);
}

static void spi_write_cmd(uint8_t cmd) {
    gpio_put(pin_dc, 0);  // Command mode
    cs_select();
    spi_write_blocking(spi, &cmd, 1);
    cs_deselect();
}

static void spi_write_data(const uint8_t* data, size_t len) {
    gpio_put(pin_dc, 1);  // Data mode
    cs_select();
    spi_write_blocking(spi, data, len);
    cs_deselect();
}

void display_spi_init(const display_config_t* config) {
    spi = (config->spi_inst == 0) ? spi0 : spi1;
    pin_cs = config->pin_cs;
    pin_dc = config->pin_dc;

    // Initialize SPI at 10MHz
    spi_init(spi, 10 * 1000 * 1000);
    gpio_set_function(config->pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(config->pin_mosi, GPIO_FUNC_SPI);

    // Initialize control pins
    gpio_init(pin_cs);
    gpio_set_dir(pin_cs, GPIO_OUT);
    gpio_put(pin_cs, 1);

    gpio_init(pin_dc);
    gpio_set_dir(pin_dc, GPIO_OUT);

    // Hardware reset
    gpio_init(config->pin_rst);
    gpio_set_dir(config->pin_rst, GPIO_OUT);
    gpio_put(config->pin_rst, 1);
    sleep_ms(10);
    gpio_put(config->pin_rst, 0);
    sleep_ms(10);
    gpio_put(config->pin_rst, 1);
    sleep_ms(10);

    // Register transport
    display_set_transport(spi_write_cmd, spi_write_data);
    display_set_col_offset(2);  // SH1106 uses 132-column RAM, offset by 2
}
