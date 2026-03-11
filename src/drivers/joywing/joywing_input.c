// joywing_input.c - Adafruit Joy FeatherWing Input Interface
//
// Reads joystick (2x ADC) and buttons (GPIO bulk) via seesaw protocol.
// Submits as INPUT_SOURCE_GPIO with dev_addr 0xE0.

#include "joywing_input.h"
#include "drivers/seesaw/seesaw.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

// Joy FeatherWing button pin assignments (seesaw GPIO numbers)
#define JOYWING_BTN_A      6
#define JOYWING_BTN_B      7
#define JOYWING_BTN_X      9
#define JOYWING_BTN_Y      10
#define JOYWING_BTN_SELECT 14

// Joystick ADC channels (seesaw SAMD09 pin-to-channel mapping)
// Physical pin 2 → ADC channel 0, pin 3 → channel 1, pin 4 → channel 2, etc.
#define JOYWING_ADC_X      1   // Pin 3 (JOYSTICK_H) → ADC channel 1
#define JOYWING_ADC_Y      0   // Pin 2 (JOYSTICK_V) → ADC channel 0

// Button pin mask for GPIO bulk read
#define JOYWING_BTN_MASK ((1u << JOYWING_BTN_A) | (1u << JOYWING_BTN_B) | \
                          (1u << JOYWING_BTN_X) | (1u << JOYWING_BTN_Y) | \
                          (1u << JOYWING_BTN_SELECT))

// Device address for router (unique, in the 0xE0+ range for I2C devices)
#define JOYWING_DEV_ADDR 0xE0

// State
static joywing_config_t joywing_cfg;
static bool config_set = false;
static seesaw_device_t seesaw;
static platform_i2c_t i2c_bus;
static input_event_t joywing_event;
static bool initialized = false;

void joywing_input_init_config(const joywing_config_t* config)
{
    joywing_cfg = *config;
    config_set = true;
}

static void joywing_init(void)
{
    if (!config_set) {
        printf("[joywing] ERROR: config not set, call joywing_input_init_config() first\n");
        return;
    }

    // Initialize I2C bus
    platform_i2c_config_t i2c_cfg = {
        .bus = joywing_cfg.i2c_bus,
        .sda_pin = joywing_cfg.sda_pin,
        .scl_pin = joywing_cfg.scl_pin,
        .freq_hz = 400000,
    };
    i2c_bus = platform_i2c_init(&i2c_cfg);
    if (!i2c_bus) {
        printf("[joywing] ERROR: I2C init failed\n");
        return;
    }

    // Initialize seesaw
    seesaw_init(&seesaw, i2c_bus, SEESAW_ADDR_DEFAULT);

    // Check hardware ID
    uint8_t hw_id = seesaw_get_hw_id(&seesaw);
    printf("[joywing] Seesaw HW ID: 0x%02X\n", hw_id);

    // Configure button pins as inputs with pull-ups
    if (!seesaw_gpio_set_input_pullup(&seesaw, JOYWING_BTN_MASK)) {
        printf("[joywing] WARNING: GPIO config failed\n");
    }

    // Initialize input event with proper defaults
    init_input_event(&joywing_event);
    joywing_event.dev_addr = JOYWING_DEV_ADDR;
    joywing_event.instance = 0;
    joywing_event.type = INPUT_TYPE_GAMEPAD;
    joywing_event.transport = INPUT_TRANSPORT_I2C;

    initialized = true;
    printf("[joywing] Joy FeatherWing initialized\n");
}

static void joywing_task(void)
{
    if (!initialized) return;

    // Rate-limit polling to ~100Hz (every 10ms).
    // The seesaw ATSAMD09 needs time between I2C commands — polling at
    // 1kHz (main loop rate) overwhelms it and causes ADC read failures.
    static uint32_t last_poll = 0;
    uint32_t now = platform_time_ms();
    if (now - last_poll < 10) return;
    last_poll = now;

    // Read buttons (GPIO bulk read, active low)
    uint32_t gpio = seesaw_gpio_read_bulk(&seesaw);
    if (gpio == 0xFFFFFFFF) return;  // Read error

    uint32_t buttons = 0;
    if (!(gpio & (1u << JOYWING_BTN_A)))      buttons |= JP_BUTTON_B2;
    if (!(gpio & (1u << JOYWING_BTN_B)))      buttons |= JP_BUTTON_B1;
    if (!(gpio & (1u << JOYWING_BTN_X)))      buttons |= JP_BUTTON_B3;
    if (!(gpio & (1u << JOYWING_BTN_Y)))      buttons |= JP_BUTTON_B4;
    if (!(gpio & (1u << JOYWING_BTN_SELECT))) buttons |= JP_BUTTON_S2;

    // Small delay between GPIO and ADC commands — seesaw needs processing time
    platform_sleep_us(500);

    // Read joystick (10-bit ADC, 0-1023 → 0-255)
    uint16_t raw_x = seesaw_adc_read(&seesaw, JOYWING_ADC_X);
    if (raw_x == SEESAW_ADC_ERROR) goto submit;  // ADC failed, keep last analog values

    platform_sleep_us(500);

    uint16_t raw_y = seesaw_adc_read(&seesaw, JOYWING_ADC_Y);
    if (raw_y == SEESAW_ADC_ERROR) goto submit;  // ADC failed, keep last analog values

    // Scale 10-bit (0-1023) to 8-bit (0-255)
    joywing_event.analog[ANALOG_LX] = (uint8_t)(raw_x >> 2);
    joywing_event.analog[ANALOG_LY] = (uint8_t)(raw_y >> 2);

submit:
    joywing_event.buttons = buttons;
    router_submit_input(&joywing_event);
}

static bool joywing_is_connected(void)
{
    return initialized;
}

static uint8_t joywing_get_device_count(void)
{
    return initialized ? 1 : 0;
}

const InputInterface joywing_input_interface = {
    .name = "JoyWing",
    .source = INPUT_SOURCE_GPIO,
    .init = joywing_init,
    .task = joywing_task,
    .is_connected = joywing_is_connected,
    .get_device_count = joywing_get_device_count,
};
