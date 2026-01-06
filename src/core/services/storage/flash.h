// core/services/storage/flash.h - Persistent settings storage in flash memory
//
// Uses journaled storage for BT-safe writes:
// - 4KB sector = 16 x 256-byte slots (ring buffer)
// - Each save writes to next empty slot (page program only, ~1ms)
// - Sector erase (~45ms) only when full AND BT is idle
// - Sequence number identifies newest entry
//
// Settings persist across power cycles and firmware updates (unless flash is erased).

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>

// Settings structure stored in flash (256 bytes = 1 flash page)
// 16 entries fit in one 4KB sector for journaled writes
typedef struct {
    uint32_t magic;              // Validation magic number (0x47435052 = "GCPR")
    uint32_t sequence;           // Sequence number (higher = newer, 0xFFFFFFFF = empty)
    uint8_t active_profile_index; // Currently selected profile (0-N)
    uint8_t usb_output_mode;     // USB device output mode (0=HID, 1=XboxOG, etc.)
    uint8_t wiimote_orient_mode; // Wiimote orientation mode (0=Auto, 1=Horizontal, 2=Vertical)
    uint8_t reserved[245];        // Reserved for future settings (padding to 256 bytes)
} flash_t;

// Initialize flash settings system
void flash_init(void);

// Load settings from flash (returns true if valid settings found)
bool flash_load(flash_t* settings);

// Save settings to flash (debounced - actual write happens after delay)
void flash_save(const flash_t* settings);

// Force immediate save (bypasses debouncing - use sparingly)
void flash_save_now(const flash_t* settings);

// Task function to handle debounced flash writes (call from main loop)
void flash_task(void);

// Notify flash system that BT has disconnected (safe to write now)
void flash_on_bt_disconnect(void);

// Check if there's a pending flash write waiting for BT to be idle
bool flash_has_pending_write(void);

#endif // FLASH_H
