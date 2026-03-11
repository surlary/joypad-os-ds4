// joywing_input.h - Adafruit Joy FeatherWing Input Interface
//
// InputInterface for the Joy FeatherWing (seesaw-based).
// 2-axis analog joystick + 5 buttons (A, B, X, Y, Select).

#ifndef JOYWING_INPUT_H
#define JOYWING_INPUT_H

#include <stdint.h>
#include "core/input_interface.h"

// I2C configuration for the Joy FeatherWing
typedef struct {
    uint8_t i2c_bus;    // I2C bus index (0 or 1)
    uint8_t sda_pin;    // SDA GPIO pin
    uint8_t scl_pin;    // SCL GPIO pin
} joywing_config_t;

// Set configuration before init (call from app_init)
void joywing_input_init_config(const joywing_config_t* config);

// Joy FeatherWing input interface
extern const InputInterface joywing_input_interface;

#endif // JOYWING_INPUT_H
