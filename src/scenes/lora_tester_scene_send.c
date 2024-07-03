// THIS FUCNTION IS NOT WORKING
//STILL WIP NOW

#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <furi_hal_light.h>

#define UART_BAUD_RATE (9600)

static void lora_tester_scene_send_text_input_callback(void* context) {
    LoraTesterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraTesterCustomEventTextInputDone);
}

void lora_tester_scene_send_on_enter(void* context) {
    LoraTesterApp* app = context;

    uart_text_input_set_header_text(app->text_input, "Enter message to send");
    uart_text_input_set_result_callback(
        app->text_input,
        lora_tester_scene_send_text_input_callback,
        app,
        app->text_input_store,
        TEXT_INPUT_STORE_SIZE,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextInput);
}

bool lora_tester_scene_send_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraTesterCustomEventTextInputDone) {
            FuriHalSerialHandle* serial_handle =
                furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(serial_handle != NULL) {
                furi_hal_serial_init(serial_handle, UART_BAUD_RATE);
                furi_hal_serial_enable_direction(
                    serial_handle, FuriHalSerialDirectionRx | FuriHalSerialDirectionTx);

                furi_hal_serial_tx(
                    serial_handle, (uint8_t*)app->text_input_store, strlen(app->text_input_store));

                furi_hal_serial_tx_wait_complete(serial_handle);

                FURI_LOG_I("LoRaTester", "Sent: %s", app->text_input_store);

                furi_hal_serial_deinit(serial_handle);
                furi_hal_serial_control_release(serial_handle);
            } else {
                FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
            }

            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoraTesterSceneStart);
    }

    return consumed;
}

void lora_tester_scene_send_on_exit(void* context) {
    LoraTesterApp* app = context;
    UNUSED(app);
}
