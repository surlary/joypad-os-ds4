// sinput_descriptors.h - SInput USB HID descriptors
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// SInput protocol descriptors for SDL/Steam compatibility.
// Based on Handheld Legend's SInput HID specification.
//
// Features:
// - 32 buttons, 2 sticks (16-bit), 2 triggers (16-bit)
// - IMU (accelerometer + gyroscope)
// - Stereo haptic feedback
// - Player LED and RGB LED control
//
// Reference: https://docs.handheldlegend.com/s/sinput

#ifndef SINPUT_DESCRIPTORS_H
#define SINPUT_DESCRIPTORS_H

#include <stdint.h>
#include "tusb.h"

// ============================================================================
// SINPUT USB IDENTIFIERS
// ============================================================================

// Raspberry Pi commercial VID with SInput generic fallback PID
#define SINPUT_VID              0x2E8A  // Raspberry Pi
#define SINPUT_PID              0x10C6  // SInput generic
#define SINPUT_BCD_DEVICE       0x0100  // v1.0

// ============================================================================
// SINPUT BUTTON DEFINITIONS
// ============================================================================

// Button masks (32-bit across 4 bytes)
// Byte 0: Face buttons and D-pad
#define SINPUT_MASK_EAST        (1U <<  0)  // B2/Circle/B
#define SINPUT_MASK_SOUTH       (1U <<  1)  // B1/Cross/A
#define SINPUT_MASK_NORTH       (1U <<  2)  // B4/Triangle/Y
#define SINPUT_MASK_WEST        (1U <<  3)  // B3/Square/X
#define SINPUT_MASK_DU          (1U <<  4)  // D-pad Up
#define SINPUT_MASK_DD          (1U <<  5)  // D-pad Down
#define SINPUT_MASK_DL          (1U <<  6)  // D-pad Left
#define SINPUT_MASK_DR          (1U <<  7)  // D-pad Right

// Byte 1: Sticks, bumpers, triggers, paddles
#define SINPUT_MASK_L3          (1U <<  8)  // Left stick click
#define SINPUT_MASK_R3          (1U <<  9)  // Right stick click
#define SINPUT_MASK_L1          (1U << 10)  // Left bumper
#define SINPUT_MASK_R1          (1U << 11)  // Right bumper
#define SINPUT_MASK_L2          (1U << 12)  // Left trigger digital
#define SINPUT_MASK_R2          (1U << 13)  // Right trigger digital
#define SINPUT_MASK_L_PADDLE1   (1U << 14)  // Left paddle 1
#define SINPUT_MASK_R_PADDLE1   (1U << 15)  // Right paddle 1

// Byte 2: System buttons and more paddles
#define SINPUT_MASK_START       (1U << 16)  // Start/Options
#define SINPUT_MASK_BACK        (1U << 17)  // Back/Select
#define SINPUT_MASK_GUIDE       (1U << 18)  // Guide/Home
#define SINPUT_MASK_CAPTURE     (1U << 19)  // Capture/Share
#define SINPUT_MASK_L_PADDLE2   (1U << 20)  // Left paddle 2
#define SINPUT_MASK_R_PADDLE2   (1U << 21)  // Right paddle 2
#define SINPUT_MASK_TOUCHPAD1   (1U << 22)  // Touchpad 1 click
#define SINPUT_MASK_TOUCHPAD2   (1U << 23)  // Touchpad 2 click

// Byte 3: Power and misc
#define SINPUT_MASK_POWER       (1U << 24)  // Power button
#define SINPUT_MASK_MISC4       (1U << 25)
#define SINPUT_MASK_MISC5       (1U << 26)
#define SINPUT_MASK_MISC6       (1U << 27)
#define SINPUT_MASK_MISC7       (1U << 28)
#define SINPUT_MASK_MISC8       (1U << 29)
#define SINPUT_MASK_MISC9       (1U << 30)
#define SINPUT_MASK_MISC10      (1U << 31)

// ============================================================================
// SINPUT REPORT STRUCTURES
// ============================================================================

// Report IDs
#define SINPUT_REPORT_ID_INPUT    0x01
#define SINPUT_REPORT_ID_FEATURES 0x02
#define SINPUT_REPORT_ID_OUTPUT   0x03

// Output command types
#define SINPUT_CMD_HAPTIC         0x01
#define SINPUT_CMD_FEATURES       0x02
#define SINPUT_CMD_PLAYER_LED     0x03
#define SINPUT_CMD_RGB_LED        0x04

// Input Report (64 bytes including report ID)
typedef struct __attribute__((packed)) {
    uint8_t  report_id;         // 0x01
    uint8_t  plug_status;       // Plug/connection status
    uint8_t  charge_level;      // Battery charge level
    uint8_t  buttons[4];        // 32 buttons (little-endian)
    int16_t  lx;                // Left stick X (-32768 to 32767, 0 = center)
    int16_t  ly;                // Left stick Y
    int16_t  rx;                // Right stick X
    int16_t  ry;                // Right stick Y
    int16_t  lt;                // Left trigger (0 to 32767)
    int16_t  rt;                // Right trigger (0 to 32767)
    uint32_t imu_timestamp;     // IMU timestamp in microseconds
    int16_t  accel_x;           // Accelerometer X
    int16_t  accel_y;           // Accelerometer Y
    int16_t  accel_z;           // Accelerometer Z
    int16_t  gyro_x;            // Gyroscope X
    int16_t  gyro_y;            // Gyroscope Y
    int16_t  gyro_z;            // Gyroscope Z
    uint8_t  touchpad1[6];      // Touchpad 1: X(2), Y(2), Pressure(2)
    uint8_t  touchpad2[6];      // Touchpad 2: X(2), Y(2), Pressure(2)
    uint8_t  reserved[17];      // Padding to 64 bytes
} sinput_report_t;

_Static_assert(sizeof(sinput_report_t) == 64, "sinput_report_t must be 64 bytes");

// Output Report (48 bytes including report ID)
typedef struct __attribute__((packed)) {
    uint8_t  report_id;         // 0x03
    uint8_t  command;           // Command type (SINPUT_CMD_*)
    uint8_t  data[46];          // Command data
} sinput_output_t;

_Static_assert(sizeof(sinput_output_t) == 48, "sinput_output_t must be 48 bytes");

// Haptic command data (Type 2 - ERM simulation)
typedef struct __attribute__((packed)) {
    uint8_t  type;              // 2 = ERM type
    uint8_t  left_amplitude;    // Left motor amplitude (0-255)
    uint8_t  left_brake;        // Left motor brake (0 or 1)
    uint8_t  right_amplitude;   // Right motor amplitude (0-255)
    uint8_t  right_brake;       // Right motor brake (0 or 1)
} sinput_haptic_t;

// ============================================================================
// SINPUT USB DESCRIPTORS
// ============================================================================

// HID Report Descriptor for SInput
// Input: 64 bytes (Report ID 0x01)
// Feature Response: 12 bytes (Report ID 0x02)
// Output: 48 bytes (Report ID 0x03)
static const uint8_t sinput_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)

    // === Feature Response Report (12 bytes) ===
    // Sent as Input report in response to feature request command
    0x85, SINPUT_REPORT_ID_FEATURES,  // Report ID (2)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined)
    0x09, 0x05,        //   Usage (Vendor Usage 5) - Feature Response
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x0C,        //   Report Count (12)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // === Input Report (64 bytes) ===
    0x85, SINPUT_REPORT_ID_INPUT,  // Report ID (1)

    // Plug status and charge (2 bytes)
    0x05, 0x06,        //   Usage Page (Generic Device Controls)
    0x09, 0x20,        //   Usage (Battery Strength)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // 32 Buttons (4 bytes)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x20,        //   Usage Maximum (Button 32)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // Analog sticks (8 bytes) - 16-bit signed
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x30,        //   Usage (X) - Left X
    0x09, 0x31,        //   Usage (Y) - Left Y
    0x09, 0x32,        //   Usage (Z) - Right X
    0x09, 0x35,        //   Usage (Rz) - Right Y
    0x16, 0x00, 0x80,  //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // Triggers (4 bytes) - 16-bit signed
    0x09, 0x33,        //   Usage (Rx) - Left Trigger
    0x09, 0x34,        //   Usage (Ry) - Right Trigger
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // IMU timestamp (4 bytes)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined)
    0x09, 0x01,        //   Usage (Vendor Usage 1) - Timestamp
    0x15, 0x00,        //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0xFF, 0x7F,  // Logical Maximum (2147483647)
    0x75, 0x20,        //   Report Size (32)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // IMU data - Accelerometer and Gyroscope (12 bytes)
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x3D,        //   Usage (Vx) - Accel X
    0x09, 0x3E,        //   Usage (Vy) - Accel Y
    0x09, 0x3F,        //   Usage (Vz) - Accel Z
    0x09, 0x40,        //   Usage (Vbrx) - Gyro X
    0x09, 0x41,        //   Usage (Vbry) - Gyro Y
    0x09, 0x42,        //   Usage (Vbrz) - Gyro Z
    0x16, 0x00, 0x80,  //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x06,        //   Report Count (6)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // Touchpad data (12 bytes) + reserved (17 bytes) = 29 bytes vendor data
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined)
    0x09, 0x02,        //   Usage (Vendor Usage 2) - Touchpad/Reserved
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x1D,        //   Report Count (29) - touchpad + reserved
    0x81, 0x02,        //   Input (Data,Var,Abs)

    // === Output Report (48 bytes) ===
    0x85, SINPUT_REPORT_ID_OUTPUT,  // Report ID (3)

    // Command byte
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined)
    0x09, 0x03,        //   Usage (Vendor Usage 3) - Command
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x91, 0x02,        //   Output (Data,Var,Abs)

    // Command data (46 bytes)
    0x09, 0x04,        //   Usage (Vendor Usage 4) - Data
    0x95, 0x2E,        //   Report Count (46)
    0x91, 0x02,        //   Output (Data,Var,Abs)

    0xC0,              // End Collection
};

// Device descriptor
static const tusb_desc_device_t sinput_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Use class from interface
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = SINPUT_VID,
    .idProduct          = SINPUT_PID,
    .bcdDevice          = SINPUT_BCD_DEVICE,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration descriptor
// 9 (config) + 9 (interface) + 9 (HID) + 7 (EP OUT) + 7 (EP IN) = 41 bytes
#define SINPUT_CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static const uint8_t sinput_config_descriptor[] = {
    // Config descriptor
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, SINPUT_CONFIG_TOTAL_LEN, 0x80, 500),  // 500mA

    // Interface + HID + Endpoints
    // Interface descriptor
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_HID, 0, 0, 0,

    // HID descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(sinput_report_descriptor)),

    // Endpoint OUT (for output reports - rumble/LEDs)
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,

    // Endpoint IN (for input reports)
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
};

// String descriptors
#define SINPUT_MANUFACTURER  "Joypad"
#define SINPUT_PRODUCT       "Joypad (SInput)"

#endif // SINPUT_DESCRIPTORS_H
