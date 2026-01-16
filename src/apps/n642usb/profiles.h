// profiles.h - N642USB App Profiles
//
// Profile definitions for N64 to USB adapter.
// N64 L/R are digital bumpers (L1/R1), only Z should activate USB trigger (LT).

#ifndef N642USB_PROFILES_H
#define N642USB_PROFILES_H

#include "core/services/profiles/profile.h"

// ============================================================================
// DEFAULT PROFILE
// ============================================================================

static const profile_t n642usb_profiles[] = {
    {
        .name = "default",
        .description = "N64 standard - Z=LT, L/R=bumpers",
        .button_map = NULL,
        .button_map_count = 0,
        .combo_map = NULL,
        .combo_map_count = 0,
        // Clear input analog triggers (from L/R) - usbd.c will use button state instead
        // This makes Z (JP_BUTTON_L2) set USB LT to 255, while L/R stay as bumpers only
        .l2_behavior = TRIGGER_DIGITAL_ONLY,
        .r2_behavior = TRIGGER_DIGITAL_ONLY,
        .l2_threshold = 128,
        .r2_threshold = 128,
        .l2_analog_value = 0,
        .r2_analog_value = 0,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
    },
};

// ============================================================================
// PROFILE SET
// ============================================================================

static const profile_set_t n642usb_profile_set = {
    .profiles = n642usb_profiles,
    .profile_count = sizeof(n642usb_profiles) / sizeof(n642usb_profiles[0]),
    .default_index = 0,
};

#endif // N642USB_PROFILES_H
