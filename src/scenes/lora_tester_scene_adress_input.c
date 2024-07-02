#include "../lora_tester_app_i.h"

static LoRaMode original_mode;

static uint32_t get_current_baud_rate(LoraTesterApp* app) {
    return app->baud_rate;
}

static void lora_tester_scene_address_input_callback(void* context) {
    LoraTesterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraTesterCustomEventByteInputDone);
}

void lora_tester_scene_address_input_on_enter(void* context) {
    LoraTesterApp* app = context;
    ByteInput* byte_input = app->byte_input;

    original_mode = app->current_mode;
    lora_tester_set_mode(app, LoRaMode_Config);
    furi_delay_ms(100);

    byte_input_set_header_text(byte_input, "Enter Address");
    byte_input_set_result_callback(
        byte_input,
        lora_tester_scene_address_input_callback,
        NULL,
        app,
        (uint8_t*)&app->address,
        2);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewByteInput);
}

bool lora_tester_scene_address_input_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraTesterCustomEventByteInputDone) {
            uint16_t address = app->address;
            FURI_LOG_D("LoRaTester", "New address: 0x%04X", address);

            uint32_t current_baud_rate = get_current_baud_rate(app);
            FuriHalSerialHandle* serial_handle =
                furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(serial_handle == NULL) {
                FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
                return consumed;
            }

            furi_hal_serial_init(serial_handle, current_baud_rate);
            furi_hal_serial_enable_direction(
                serial_handle, FuriHalSerialDirectionRx | FuriHalSerialDirectionTx);

            uint8_t command[5] = {0xC0, 0x00, 0x02, address & 0xFF, (address >> 8) & 0xFF};
            furi_hal_serial_tx(serial_handle, command, sizeof(command));
            furi_hal_serial_tx_wait_complete(serial_handle);
            furi_delay_ms(100);

            FURI_LOG_D(
                "LoRaTester",
                "Command sent: %02X %02X %02X %02X %02X",
                command[0],
                command[1],
                command[2],
                command[3],
                command[4]);

            furi_hal_serial_deinit(serial_handle);
            furi_hal_serial_control_release(serial_handle);

            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void lora_tester_scene_address_input_on_exit(void* context) {
    LoraTesterApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);

    furi_delay_ms(100);
    lora_tester_set_mode(app, original_mode);
}