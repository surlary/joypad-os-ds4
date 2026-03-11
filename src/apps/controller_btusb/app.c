// app.c - ControllerBTUSB App Entry Point
// Modular sensor inputs → BLE gamepad + USB device output
//
// Combines the controller app's modular sensor input with usb2ble's
// BLE peripheral output. First sensor: JoyWing (seesaw I2C).

#include "app.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/button/button.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "bt/ble_output/ble_output.h"
#include "usb/usbd/usbd.h"
#include "bt/transport/bt_transport.h"
#include "core/services/leds/leds.h"
#include "core/buttons.h"
#include "platform/platform.h"

#include "tusb.h"
#include <stdio.h>

// ESP32 BLE transport
extern const bt_transport_t bt_transport_esp32;

// Post-init callback setter (declared in bt_transport_esp32.c)
typedef void (*bt_esp32_post_init_fn)(void);
extern void bt_esp32_set_post_init(bt_esp32_post_init_fn fn);

// BTstack APIs for bond management (available after ble_output_late_init)
extern void gap_delete_all_link_keys(void);
extern void gap_advertisements_enable(int enabled);
extern int le_device_db_max_count(void);
extern void le_device_db_remove(int index);

// Sensor inputs (conditional)
#ifdef SENSOR_JOYWING
#include "drivers/joywing/joywing_input.h"
#endif

// ============================================================================
// BUTTON EVENT HANDLER
// ============================================================================

// Check if USB is actively connected as a gamepad (mounted + not in CDC config mode)
static bool usb_gamepad_active(void)
{
    return tud_mounted() && usbd_get_mode() != USB_OUTPUT_MODE_CDC;
}

static void on_button_event(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_CLICK:
            printf("[app:controller_btusb] BLE: %s (%s), USB: %s (%s)\n",
                   ble_output_get_mode_name(ble_output_get_mode()),
                   ble_output_is_connected() ? "connected" : "advertising",
                   usbd_get_mode_name(usbd_get_mode()),
                   tud_mounted() ? "mounted" : "disconnected");
            break;

        case BUTTON_EVENT_DOUBLE_CLICK:
            if (ble_output_is_connected() || !usb_gamepad_active()) {
                // BLE connected or no active USB gamepad → cycle BLE mode
                ble_output_mode_t next = ble_output_get_next_mode();
                printf("[app:controller_btusb] Double-click - BLE mode → %s\n",
                       ble_output_get_mode_name(next));
                ble_output_set_mode(next);  // Saves to flash + reboots
            } else {
                // USB gamepad active, no BLE → cycle USB output mode
                usb_output_mode_t next = usbd_get_next_mode();
                printf("[app:controller_btusb] Double-click - USB mode → %s\n",
                       usbd_get_mode_name(next));
                usbd_set_mode(next);
            }
            break;

        case BUTTON_EVENT_TRIPLE_CLICK:
            // Reset USB output mode to SInput (default gamepad mode)
            printf("[app:controller_btusb] Triple-click - resetting USB mode to SInput\n");
            usbd_set_mode(USB_OUTPUT_MODE_SINPUT);
            break;

        case BUTTON_EVENT_HOLD:
            printf("[app:controller_btusb] Long press - clearing BLE bonds\n");
            gap_delete_all_link_keys();
            for (int i = 0; i < le_device_db_max_count(); i++) {
                le_device_db_remove(i);
            }
            printf("[app:controller_btusb] Bonds cleared, restarting advertising\n");
            gap_advertisements_enable(1);
            break;

        default:
            break;
    }
}

// ============================================================================
// APP INPUT INTERFACES
// ============================================================================

static const InputInterface* input_interfaces[] = {
#ifdef SENSOR_JOYWING
    &joywing_input_interface,
#endif
};

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = sizeof(input_interfaces) / sizeof(input_interfaces[0]);
    return input_interfaces;
}

// ============================================================================
// APP OUTPUT INTERFACES
// ============================================================================

static const OutputInterface* output_interfaces[] = {
    &ble_output_interface,
    &usbd_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = sizeof(output_interfaces) / sizeof(output_interfaces[0]);
    return output_interfaces;
}

// ============================================================================
// APP INITIALIZATION
// ============================================================================

void app_init(void)
{
    printf("[app:controller_btusb] Initializing ControllerBTUSB v%s\n", APP_VERSION);

    // Initialize button service
    button_init();
    button_set_callback(on_button_event);

    // Configure sensor inputs
#ifdef SENSOR_JOYWING
    joywing_config_t jw_cfg = {
        .i2c_bus = JOYWING_I2C_BUS,
        .sda_pin = JOYWING_SDA_PIN,
        .scl_pin = JOYWING_SCL_PIN,
    };
    joywing_input_init_config(&jw_cfg);
    printf("[app:controller_btusb] JoyWing sensor configured (bus=%d, SDA=%d, SCL=%d)\n",
           JOYWING_I2C_BUS, JOYWING_SDA_PIN, JOYWING_SCL_PIN);
#endif

    // Configure router: merge all sensor inputs to both BLE + USB outputs
    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_BLE_PERIPHERAL] = 1,
            [OUTPUT_TARGET_USB_DEVICE] = 1,
        },
        .merge_all_inputs = true,
        .transform_flags = TRANSFORM_FLAGS,
    };
    router_init(&router_cfg);

    // Route: GPIO (sensors) → BLE Peripheral
    router_add_route(INPUT_SOURCE_GPIO, OUTPUT_TARGET_BLE_PERIPHERAL, 0);

    // Route: GPIO (sensors) → USB Device (CDC config + wired gamepad)
    router_add_route(INPUT_SOURCE_GPIO, OUTPUT_TARGET_USB_DEVICE, 0);

    // Configure player management
    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    // Load BLE output mode from flash BEFORE starting BTstack task.
    // The BTstack FreeRTOS task calls ble_output_late_init() asynchronously,
    // which needs current_mode to already be set from flash settings.
    // (main.c output init loop will call this again — harmless double-init.)
    ble_output_init();

    // Initialize ESP32 BLE transport in peripheral mode.
    // Set post-init callback so ble_output_late_init() runs in the BTstack task context.
    bt_esp32_set_post_init(ble_output_late_init);
    bt_init(&bt_transport_esp32);

    printf("[app:controller_btusb] Initialization complete\n");
    printf("[app:controller_btusb]   Routing: Sensors → BLE Peripheral + USB Device\n");
    printf("[app:controller_btusb]   Player slots: %d\n", MAX_PLAYER_SLOTS);
}

// ============================================================================
// APP TASK (Called from main loop)
// ============================================================================

void app_task(void)
{
    // Process button input
    button_task();

    // Process BLE transport
    bt_task();

    // NeoPixel: show connection state and active output mode color
    //  - BLE connected → solid with BLE mode color
    //  - USB gamepad active (no BLE) → solid with USB mode color
    //  - Neither → breathing with BLE mode color (advertising)
    bool ble_conn = ble_output_is_connected();
    bool usb_active = usb_gamepad_active();
    leds_set_connected_devices((ble_conn || usb_active) ? 1 : 0);

    // Track state changes for LED color updates
    static bool last_ble_conn = false;
    static bool last_usb_active = false;
    static ble_output_mode_t last_ble_mode = BLE_MODE_COUNT;
    static usb_output_mode_t last_usb_mode = USB_OUTPUT_MODE_COUNT;

    ble_output_mode_t ble_mode = ble_output_get_mode();
    usb_output_mode_t usb_mode = usbd_get_mode();

    if (ble_conn != last_ble_conn || usb_active != last_usb_active ||
        ble_mode != last_ble_mode || usb_mode != last_usb_mode) {
        last_ble_conn = ble_conn;
        last_usb_active = usb_active;
        last_ble_mode = ble_mode;
        last_usb_mode = usb_mode;

        uint8_t r, g, b;
        if (ble_conn) {
            ble_output_get_mode_color(ble_mode, &r, &g, &b);
        } else if (usb_active) {
            usbd_get_mode_color(usb_mode, &r, &g, &b);
        } else {
            ble_output_get_mode_color(ble_mode, &r, &g, &b);
        }
        leds_set_color(r, g, b);
    }
}
