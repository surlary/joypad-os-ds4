// ws2812.h - NeoPixel LED Control
//
// Controls WS2812 RGB LED for status indication.

#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>
#include <stdbool.h>

// Initialize NeoPixel LED
void neopixel_init(void);

// Update NeoPixel LED pattern based on player count
// pat: number of connected players (0 = no players, shows idle pattern)
void neopixel_task(int pat);

// Trigger profile indicator blink pattern
// profile_index: 0-3 (blinks profile_index + 1 times)
void neopixel_indicate_profile(uint8_t profile_index);

// Check if profile indicator is currently active
bool neopixel_is_indicating(void);

// Set custom per-LED colors from GPIO config
// colors: array of [n][3] RGB values, count: number of LEDs
void neopixel_set_custom_colors(const uint8_t colors[][3], uint8_t count);

// Check if custom colors are active
bool neopixel_has_custom_colors(void);

// Set bitmask of LEDs that pulse with breathing animation
// bit N set = LED N pulses, 0 = all solid
void neopixel_set_pulse_mask(uint16_t mask);

// Set bitmask of LEDs currently pressed (shown as bright white)
void neopixel_set_press_mask(uint16_t mask);

// Set override color for mode indication (USB output apps)
// When set, overrides pattern table: pulses when pat=0, solid when pat>0
void neopixel_set_override_color(uint8_t r, uint8_t g, uint8_t b);

#endif // WS2812_H
