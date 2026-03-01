// display_transport.h - Display transport abstraction (internal)
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Internal header for pluggable display transports (SPI, I2C).
// Not part of the public display API.

#ifndef DISPLAY_TRANSPORT_H
#define DISPLAY_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

// Set transport callbacks for sending commands and data to the display.
// Must be called before display init sequence runs.
void display_set_transport(void (*write_cmd)(uint8_t),
                           void (*write_data)(const uint8_t*, size_t));

// Set column offset for page writes (SH1106=2, SH1107=0)
void display_set_col_offset(uint8_t offset);

#endif // DISPLAY_TRANSPORT_H
