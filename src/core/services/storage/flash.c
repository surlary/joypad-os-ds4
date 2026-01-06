// core/services/storage/flash.c - Persistent settings storage in flash memory
//
// Uses journaled storage for BT-safe writes:
// - 4KB sector = 16 x 256-byte slots (ring buffer)
// - Each save writes to next empty slot (page program only, ~1ms)
// - Sector erase (~45ms) only when full AND BT is idle
// - This allows settings to be saved during active BT connections

#include "core/services/storage/flash.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "tusb.h"
#include <string.h>
#include <stdio.h>

// BT connection check (weak symbol - overridden when BT is enabled)
__attribute__((weak)) uint8_t btstack_classic_get_connection_count(void) { return 0; }

// Helper to flush debug output before critical sections
static void flush_output(void)
{
#if CFG_TUD_ENABLED
    tud_task();
    sleep_ms(20);
    tud_task();
#else
    sleep_ms(20);
#endif
}

// Flash memory layout
// - RP2040/RP2350 flash is memory-mapped at XIP_BASE (0x10000000)
// - BTstack uses 8KB (2 sectors) for Bluetooth bond storage
// - We use the sector before BTstack for settings storage
// - Flash writes require erasing entire 4KB sectors
// - Flash page writes are 256-byte aligned
//
// Layout differs by platform:
// - RP2040: BTstack at end of flash (last 2 sectors)
// - RP2350 (A2): BTstack 1 sector from end (due to RP2350-E10 errata)

#define SETTINGS_MAGIC 0x47435052  // "GCPR" - GameCube Profile
#define BTSTACK_FLASH_SIZE (FLASH_SECTOR_SIZE * 2)  // 8KB for BTstack

#if PICO_RP2350 && PICO_RP2350_A2_SUPPORTED
// RP2350 layout: [... | settings | btstack (2 sectors) | reserved (1 sector)]
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE - BTSTACK_FLASH_SIZE - FLASH_SECTOR_SIZE)
#else
// RP2040 layout: [... | settings | btstack (2 sectors)]
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - BTSTACK_FLASH_SIZE - FLASH_SECTOR_SIZE)
#endif

// Journal configuration
#define JOURNAL_SLOT_SIZE FLASH_PAGE_SIZE  // 256 bytes per slot
#define JOURNAL_SLOT_COUNT (FLASH_SECTOR_SIZE / JOURNAL_SLOT_SIZE)  // 16 slots
#define SAVE_DEBOUNCE_MS 5000  // Wait 5 seconds after last change before writing

// Pending save state
static bool save_pending = false;
static bool erase_pending = false;  // Sector full, waiting for BT idle to erase
static absolute_time_t last_change_time;
static flash_t pending_settings;
static uint32_t current_sequence = 0;  // Current sequence number

// Check if BT has active connections
static bool bt_is_active(void)
{
    return btstack_classic_get_connection_count() > 0;
}

// Get pointer to a journal slot
static const flash_t* get_slot(uint8_t slot_index)
{
    return (const flash_t*)(XIP_BASE + FLASH_TARGET_OFFSET + (slot_index * JOURNAL_SLOT_SIZE));
}

// Check if a slot is empty (erased state = 0xFFFFFFFF)
static bool is_slot_empty(uint8_t slot_index)
{
    const flash_t* slot = get_slot(slot_index);
    return slot->sequence == 0xFFFFFFFF;
}

// Find the newest valid entry (highest sequence number)
// Returns slot index, or -1 if no valid entries
static int find_newest_slot(void)
{
    int newest_slot = -1;
    uint32_t highest_seq = 0;

    for (uint8_t i = 0; i < JOURNAL_SLOT_COUNT; i++) {
        const flash_t* slot = get_slot(i);

        // Check for valid magic and non-empty sequence
        if (slot->magic == SETTINGS_MAGIC && slot->sequence != 0xFFFFFFFF) {
            if (newest_slot == -1 || slot->sequence > highest_seq) {
                highest_seq = slot->sequence;
                newest_slot = i;
            }
        }
    }

    return newest_slot;
}

// Find the next empty slot
// Returns slot index, or -1 if sector is full
static int find_empty_slot(void)
{
    for (uint8_t i = 0; i < JOURNAL_SLOT_COUNT; i++) {
        if (is_slot_empty(i)) {
            return i;
        }
    }
    return -1;  // Sector full
}

void flash_init(void)
{
    save_pending = false;
    erase_pending = false;

    // Find current sequence number from flash
    int newest = find_newest_slot();
    if (newest >= 0) {
        current_sequence = get_slot(newest)->sequence;
    } else {
        current_sequence = 0;
    }
}

// Load settings from flash (returns true if valid settings found)
bool flash_load(flash_t* settings)
{
    int newest = find_newest_slot();

    if (newest < 0) {
        return false;  // No valid settings in flash
    }

    // Copy settings from flash to RAM
    const flash_t* slot = get_slot(newest);
    memcpy(settings, slot, sizeof(flash_t));
    current_sequence = slot->sequence;

    return true;
}

// Save settings to flash (debounced - actual write happens after delay)
void flash_save(const flash_t* settings)
{
    // Store settings and mark as pending
    memcpy(&pending_settings, settings, sizeof(flash_t));
    pending_settings.magic = SETTINGS_MAGIC;
    save_pending = true;
    last_change_time = get_absolute_time();
}

// Page program worker - only programs one page, no erase (~1ms)
// This is safe during BT as it only takes ~1ms
typedef struct {
    uint32_t offset;
    const uint8_t* data;
} page_program_params_t;

static void __no_inline_not_in_flash_func(page_program_worker)(void* param)
{
    page_program_params_t* p = (page_program_params_t*)param;
    flash_range_program(p->offset, p->data, FLASH_PAGE_SIZE);
}

// Sector erase worker - erases entire sector (~45ms)
// NOT safe during BT - only call when BT is idle
static void __no_inline_not_in_flash_func(sector_erase_worker)(void* param)
{
    (void)param;
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
}

// Write a single page to flash (BT-safe, ~1ms)
static bool flash_write_page(uint8_t slot_index, const flash_t* settings)
{
    static flash_t write_buffer;  // Static to persist during flash ops
    memcpy(&write_buffer, settings, sizeof(flash_t));

    uint32_t offset = FLASH_TARGET_OFFSET + (slot_index * JOURNAL_SLOT_SIZE);

    page_program_params_t params = {
        .offset = offset,
        .data = (const uint8_t*)&write_buffer
    };

    // Try flash_safe_execute first
    int result = flash_safe_execute(page_program_worker, &params, UINT32_MAX);

    if (result != PICO_OK) {
        // Fallback: direct write with interrupts disabled briefly
        uint32_t ints = save_and_disable_interrupts();
        flash_range_program(offset, (const uint8_t*)&write_buffer, FLASH_PAGE_SIZE);
        restore_interrupts(ints);
    }

    return true;
}

// Erase the sector (NOT BT-safe, ~45ms)
static void flash_erase_sector(void)
{
    printf("[flash] Erasing sector at offset 0x%X...\n", FLASH_TARGET_OFFSET);
    flush_output();

    // Try flash_safe_execute first
    int result = flash_safe_execute(sector_erase_worker, NULL, UINT32_MAX);

    if (result != PICO_OK) {
        printf("[flash] flash_safe_execute failed (%d), trying direct erase...\n", result);
        flush_output();

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
        restore_interrupts(ints);
    }

    printf("[flash] Sector erase complete\n");
}

// Force immediate save (bypasses debouncing)
void flash_save_now(const flash_t* settings)
{
    static flash_t write_settings;
    memcpy(&write_settings, settings, sizeof(flash_t));
    write_settings.magic = SETTINGS_MAGIC;
    write_settings.sequence = ++current_sequence;

    // Find next empty slot
    int slot = find_empty_slot();

    if (slot < 0) {
        // Sector full - need to erase first
        if (bt_is_active()) {
            // BT active - defer erase, keep settings pending
            printf("[flash] Sector full, BT active - deferring erase\n");
            erase_pending = true;
            memcpy(&pending_settings, &write_settings, sizeof(flash_t));
            save_pending = true;
            return;
        }

        // BT idle - safe to erase
        flash_erase_sector();
        slot = 0;
        erase_pending = false;
    }

    printf("[flash] Writing to slot %d (seq=%lu) at offset 0x%lX\n",
           slot, (unsigned long)write_settings.sequence,
           (unsigned long)(FLASH_TARGET_OFFSET + slot * JOURNAL_SLOT_SIZE));

    flash_write_page(slot, &write_settings);

    // Verify the write
    const flash_t* verify = get_slot(slot);
    printf("[flash] Verify: magic=0x%08lX, seq=%lu, profile=%d, usb_mode=%d, orient=%d\n",
           (unsigned long)verify->magic, (unsigned long)verify->sequence,
           verify->active_profile_index, verify->usb_output_mode,
           verify->wiimote_orient_mode);

    save_pending = false;
}

// Task function to handle debounced flash writes (call from main loop)
void flash_task(void)
{
    // Check if we have a pending erase waiting for BT to be idle
    if (erase_pending && !bt_is_active()) {
        printf("[flash] BT now idle, performing deferred erase and write\n");
        flash_erase_sector();
        erase_pending = false;

        // Now write the pending settings to slot 0
        if (save_pending) {
            pending_settings.sequence = ++current_sequence;
            printf("[flash] Writing deferred settings to slot 0\n");
            flash_write_page(0, &pending_settings);
            save_pending = false;
        }
        return;
    }

    if (!save_pending) {
        return;
    }

    // Check if debounce time has elapsed
    int64_t time_since_change = absolute_time_diff_us(last_change_time, get_absolute_time());
    if (time_since_change >= (SAVE_DEBOUNCE_MS * 1000)) {
        flash_save_now(&pending_settings);
    }
}

// Called when BT disconnects - check if we have pending erases
void flash_on_bt_disconnect(void)
{
    if (erase_pending && !bt_is_active()) {
        printf("[flash] BT disconnected, performing deferred erase\n");
        flash_erase_sector();
        erase_pending = false;

        // Write pending settings to slot 0
        if (save_pending) {
            pending_settings.sequence = ++current_sequence;
            printf("[flash] Writing deferred settings to slot 0\n");
            flash_write_page(0, &pending_settings);
            save_pending = false;
        }
    }
}

// Check if there's a pending write/erase waiting
bool flash_has_pending_write(void)
{
    return save_pending || erase_pending;
}
