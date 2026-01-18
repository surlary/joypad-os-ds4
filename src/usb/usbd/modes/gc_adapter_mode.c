// gc_adapter_mode.c - GameCube Adapter USB device mode
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Emulates Nintendo GameCube Controller Adapter for Wii U/Switch.
// Supports up to 4 controllers via single USB interface.

#include "tusb_config.h"

#if CFG_TUD_GC_ADAPTER

#include "tusb.h"
#include "../usbd_mode.h"
#include "../usbd.h"
#include "descriptors/gc_adapter_descriptors.h"
#include "core/buttons.h"
#include "core/output_interface.h"
#include <string.h>

// ============================================================================
// STATE
// ============================================================================

static gc_adapter_in_report_t gc_adapter_report;
static gc_adapter_out_report_t gc_adapter_rumble;
static bool gc_adapter_rumble_available = false;

// ============================================================================
// MODE INTERFACE IMPLEMENTATION
// ============================================================================

static void gc_adapter_mode_init(void)
{
    memset(&gc_adapter_report, 0, sizeof(gc_adapter_in_report_t));
    gc_adapter_report.report_id = GC_ADAPTER_REPORT_ID_INPUT;

    // Initialize all ports as disconnected with neutral analog values
    for (int i = 0; i < 4; i++) {
        gc_adapter_report.port[i].connected = GC_ADAPTER_PORT_NONE;
        gc_adapter_report.port[i].type = GC_ADAPTER_TYPE_NONE;
        gc_adapter_report.port[i].stick_x = 128;
        gc_adapter_report.port[i].stick_y = 128;
        gc_adapter_report.port[i].cstick_x = 128;
        gc_adapter_report.port[i].cstick_y = 128;
    }

    memset(&gc_adapter_rumble, 0, sizeof(gc_adapter_out_report_t));
    gc_adapter_rumble_available = false;
}

static bool gc_adapter_mode_is_ready(void)
{
    return tud_hid_ready();
}

static bool gc_adapter_mode_send_report(uint8_t player_index,
                                         const input_event_t* event,
                                         const profile_output_t* profile_out,
                                         uint32_t buttons)
{
    (void)event;

    // Map player to port (player 0-3 maps to port 0-3)
    uint8_t port = player_index;
    if (port >= 4) port = 0;

    // Mark port as connected with wired controller
    gc_adapter_report.port[port].connected = GC_ADAPTER_PORT_WIRED >> 4;
    gc_adapter_report.port[port].type = GC_ADAPTER_TYPE_NORMAL;

    // Map buttons to GC adapter format
    gc_adapter_report.port[port].a = (buttons & JP_BUTTON_B2) ? 1 : 0;  // GC A = B2
    gc_adapter_report.port[port].b = (buttons & JP_BUTTON_B1) ? 1 : 0;  // GC B = B1
    gc_adapter_report.port[port].x = (buttons & JP_BUTTON_B4) ? 1 : 0;  // GC X = B4
    gc_adapter_report.port[port].y = (buttons & JP_BUTTON_B3) ? 1 : 0;  // GC Y = B3

    gc_adapter_report.port[port].z = (buttons & JP_BUTTON_R1) ? 1 : 0;  // GC Z = R1
    gc_adapter_report.port[port].l = (buttons & JP_BUTTON_L2) ? 1 : 0;  // GC L = L2
    gc_adapter_report.port[port].r = (buttons & JP_BUTTON_R2) ? 1 : 0;  // GC R = R2
    gc_adapter_report.port[port].start = (buttons & JP_BUTTON_S2) ? 1 : 0;

    gc_adapter_report.port[port].dpad_up = (buttons & JP_BUTTON_DU) ? 1 : 0;
    gc_adapter_report.port[port].dpad_down = (buttons & JP_BUTTON_DD) ? 1 : 0;
    gc_adapter_report.port[port].dpad_left = (buttons & JP_BUTTON_DL) ? 1 : 0;
    gc_adapter_report.port[port].dpad_right = (buttons & JP_BUTTON_DR) ? 1 : 0;

    // Analog sticks (GC uses 0-255 with 128 center, Y inverted from HID)
    // GC: 0=down, 255=up (inverted from standard HID)
    gc_adapter_report.port[port].stick_x = profile_out->left_x;
    gc_adapter_report.port[port].stick_y = 255 - profile_out->left_y;  // Invert Y
    gc_adapter_report.port[port].cstick_x = profile_out->right_x;
    gc_adapter_report.port[port].cstick_y = 255 - profile_out->right_y;  // Invert Y

    // Analog triggers (0-255)
    gc_adapter_report.port[port].trigger_l = profile_out->l2_analog;
    gc_adapter_report.port[port].trigger_r = profile_out->r2_analog;

    // Fall back to digital if analog is 0 but button pressed
    if (gc_adapter_report.port[port].trigger_l == 0 && (buttons & JP_BUTTON_L2))
        gc_adapter_report.port[port].trigger_l = 0xFF;
    if (gc_adapter_report.port[port].trigger_r == 0 && (buttons & JP_BUTTON_R2))
        gc_adapter_report.port[port].trigger_r = 0xFF;

    // Send via HID with report ID 0x21 - tud_hid_report prepends report_id to data
    // So we send 36 bytes of port data, and TinyUSB adds the 0x21 prefix = 37 bytes total
    return tud_hid_report(GC_ADAPTER_REPORT_ID_INPUT, gc_adapter_report.port, sizeof(gc_adapter_report.port));
}

static void gc_adapter_mode_handle_output(uint8_t report_id, const uint8_t* data, uint16_t len)
{
    // TinyUSB may pass report_id=0 with report ID as first byte of data
    if (report_id == 0 && len > 0) {
        report_id = data[0];
        data++;
        len--;
    }

    // Rumble output - Report ID 0x11 (4 bytes: one per port)
    if (report_id == GC_ADAPTER_REPORT_ID_RUMBLE && len >= 4) {
        gc_adapter_rumble.report_id = GC_ADAPTER_REPORT_ID_RUMBLE;
        memcpy(gc_adapter_rumble.rumble, data, 4);
        gc_adapter_rumble_available = true;
        return;
    }

    // Init command - Report ID 0x13 (no data, just ack)
    if (report_id == GC_ADAPTER_REPORT_ID_INIT) {
        // Init command received - adapter is now active
        return;
    }
}

static uint8_t gc_adapter_mode_get_rumble(void)
{
    // GC adapter has binary rumble per port - check if any port has rumble
    for (int i = 0; i < 4; i++) {
        if (gc_adapter_rumble.rumble[i]) return 0xFF;
    }
    return 0;
}

// Get per-port rumble for a specific player/port
uint8_t gc_adapter_mode_get_port_rumble(uint8_t port)
{
    if (port >= 4) return 0;
    return gc_adapter_rumble.rumble[port] ? 0xFF : 0;
}

static bool gc_adapter_mode_get_feedback(output_feedback_t* fb)
{
    if (!fb) return false;

    // GC Adapter has binary rumble per port
    // For the simple interface, combine all ports
    uint8_t rumble = 0;
    for (int i = 0; i < 4; i++) {
        if (gc_adapter_rumble.rumble[i]) {
            rumble = 0xFF;
            break;
        }
    }

    fb->rumble_left = rumble;
    fb->rumble_right = rumble;
    fb->led_player = 0;
    fb->led_r = fb->led_g = fb->led_b = 0;
    fb->dirty = gc_adapter_rumble_available;

    return gc_adapter_rumble_available;
}

static const uint8_t* gc_adapter_mode_get_device_descriptor(void)
{
    return (const uint8_t*)&gc_adapter_device_descriptor;
}

static const uint8_t* gc_adapter_mode_get_config_descriptor(void)
{
    return gc_adapter_config_descriptor;
}

static const uint8_t* gc_adapter_mode_get_report_descriptor(void)
{
    return gc_adapter_report_descriptor;
}

// ============================================================================
// MODE EXPORT
// ============================================================================

const usbd_mode_t gc_adapter_mode = {
    .name = "GC Adapter",
    .mode = USB_OUTPUT_MODE_GC_ADAPTER,

    .get_device_descriptor = gc_adapter_mode_get_device_descriptor,
    .get_config_descriptor = gc_adapter_mode_get_config_descriptor,
    .get_report_descriptor = gc_adapter_mode_get_report_descriptor,

    .init = gc_adapter_mode_init,
    .send_report = gc_adapter_mode_send_report,
    .is_ready = gc_adapter_mode_is_ready,

    .handle_output = gc_adapter_mode_handle_output,
    .get_rumble = gc_adapter_mode_get_rumble,
    .get_feedback = gc_adapter_mode_get_feedback,
    .get_report = NULL,

    .get_class_driver = NULL,  // Uses built-in HID class driver
    .task = NULL,
};

#endif // CFG_TUD_GC_ADAPTER
