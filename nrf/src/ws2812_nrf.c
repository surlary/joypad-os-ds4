// ws2812_nrf.c - RGB LED driver for nRF52840 boards
//
// Maps NeoPixel API to discrete GPIO LEDs with color thresholding
// and blink-based pulsing.
//
// XIAO nRF52840:   3 LEDs (R=P0.26, G=P0.30, B=P0.06) on gpio0, active LOW
// Feather nRF52840: 2 LEDs (R=P1.15, B=P1.10) on gpio1, active HIGH

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#ifdef BOARD_FEATHER_NRF52840
// Feather nRF52840 Express: Red=P1.15, Blue=P1.10, active HIGH, no green
#define LED_RED_PIN   15  // P1.15
#define LED_BLUE_PIN  10  // P1.10
#define LED_PORT_LABEL DT_NODELABEL(gpio1)
#define LED_ACTIVE_HIGH 1
#else
// Seeed XIAO nRF52840: R=P0.26, G=P0.30, B=P0.06, active LOW
#define LED_RED_PIN   26  // P0.26
#define LED_GREEN_PIN 30  // P0.30
#define LED_BLUE_PIN   6  // P0.06
#define LED_PORT_LABEL DT_NODELABEL(gpio0)
#define LED_ACTIVE_HIGH 0
#endif

static const struct device *led_port;

// Override color (set by app for USB mode indication)
static uint8_t override_r = 0, override_g = 0, override_b = 0;
static bool has_override_color = false;

// Timing
static uint32_t last_update_ms = 0;
static int tic = 0;

// Profile indicator state
typedef enum {
    LED_IDLE,
    LED_BLINK_ON,
    LED_BLINK_OFF,
} led_state_t;

static led_state_t led_state = LED_IDLE;
static uint8_t blinks_remaining = 0;
static uint32_t state_change_ms = 0;

#define BLINK_OFF_MS    200
#define BLINK_ON_MS     100
#define PULSE_PERIOD_MS 1000

// ============================================================================
// GPIO HELPERS
// ============================================================================

// Set LEDs at once. Polarity handled by LED_ACTIVE_HIGH.
static void set_rgb(bool r, bool g, bool b)
{
    if (!led_port) return;
#if LED_ACTIVE_HIGH
    gpio_pin_set(led_port, LED_RED_PIN, r ? 1 : 0);
    gpio_pin_set(led_port, LED_BLUE_PIN, b ? 1 : 0);
#else
    gpio_pin_set(led_port, LED_RED_PIN, r ? 0 : 1);
    gpio_pin_set(led_port, LED_GREEN_PIN, g ? 0 : 1);
    gpio_pin_set(led_port, LED_BLUE_PIN, b ? 0 : 1);
#endif
}

// Threshold RGB values to on/off per channel
static void set_color(uint8_t r, uint8_t g, uint8_t b)
{
    set_rgb(r > 0, g > 0, b > 0);
}

static void set_off(void)
{
    set_rgb(false, false, false);
}

// ============================================================================
// PUBLIC API
// ============================================================================

void neopixel_init(void)
{
    led_port = DEVICE_DT_GET(LED_PORT_LABEL);
    if (!device_is_ready(led_port)) {
        printf("[led_nrf] GPIO port not ready\n");
        led_port = NULL;
        return;
    }

#if LED_ACTIVE_HIGH
    // Active high: start LOW = LED off
    gpio_pin_configure(led_port, LED_RED_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(led_port, LED_BLUE_PIN, GPIO_OUTPUT_LOW);
    printf("[led_nrf] LEDs initialized (R=P1.%d B=P1.%d, active high)\n",
           LED_RED_PIN, LED_BLUE_PIN);
#else
    // Active low: start HIGH = LED off
    gpio_pin_configure(led_port, LED_RED_PIN, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(led_port, LED_GREEN_PIN, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(led_port, LED_BLUE_PIN, GPIO_OUTPUT_HIGH);
    printf("[led_nrf] RGB LEDs initialized (R=P0.%d G=P0.%d B=P0.%d, active low)\n",
           LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);
#endif
}

void neopixel_set_override_color(uint8_t r, uint8_t g, uint8_t b)
{
    override_r = r;
    override_g = g;
    override_b = b;
    has_override_color = true;
}

void neopixel_indicate_profile(uint8_t profile_index)
{
    if (led_state == LED_IDLE) {
        blinks_remaining = profile_index + 1;
        led_state = LED_BLINK_OFF;
        state_change_ms = k_uptime_get_32();
    }
}

bool neopixel_is_indicating(void)
{
    return led_state != LED_IDLE;
}

void neopixel_task(int pat)
{
    if (!led_port) return;

    uint32_t now = k_uptime_get_32();

    // Profile indicator state machine
    if (led_state != LED_IDLE) {
        uint32_t elapsed = now - state_change_ms;

        switch (led_state) {
            case LED_BLINK_OFF:
                set_off();
                if (elapsed >= BLINK_OFF_MS) {
                    blinks_remaining--;
                    if (blinks_remaining > 0) {
                        led_state = LED_BLINK_ON;
                    } else {
                        led_state = LED_IDLE;
                    }
                    state_change_ms = now;
                }
                break;

            case LED_BLINK_ON:
                if (has_override_color) {
                    set_color(override_r, override_g, override_b);
                } else {
                    set_rgb(true, true, true);
                }
                if (elapsed >= BLINK_ON_MS) {
                    led_state = LED_BLINK_OFF;
                    state_change_ms = now;
                }
                break;

            default:
                led_state = LED_IDLE;
                break;
        }
        return;
    }

    // Rate limit updates (~50Hz)
    if (now - last_update_ms < 20) return;
    last_update_ms = now;
    tic++;

    // Override color mode (USB output mode indication)
    if (has_override_color) {
        if (pat == 0) {
            // Pulse: blink on/off ~1Hz
            bool phase = (tic % (PULSE_PERIOD_MS / 20)) < (PULSE_PERIOD_MS / 40);
            if (phase) {
                set_color(override_r, override_g, override_b);
            } else {
                set_off();
            }
        } else {
            // Solid: connected
            set_color(override_r, override_g, override_b);
        }
        return;
    }

    // Default: no override color set
    if (pat == 0) {
        // No connection: pulse blue
        bool phase = (tic % (PULSE_PERIOD_MS / 20)) < (PULSE_PERIOD_MS / 40);
        set_rgb(false, false, phase);
    } else {
        // Connected: solid blue
        set_rgb(false, false, true);
    }
}
