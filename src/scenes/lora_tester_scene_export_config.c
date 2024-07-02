#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/path.h>

#define CONFIG_FILE_DIRECTORY "/ext/LoRa_Setting"
#define CONFIG_FILE_EXTENSION ".ini"
#define LORA_RX_BUFFER_SIZE 256
#define LORA_SEARCH_TIMEOUT 3000

static uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];
static size_t rx_buffer_index = 0;
static LoRaMode original_mode;

static uint32_t get_current_baud_rate(LoraTesterApp* app) {
    return app->baud_rate;
}

static void lora_tester_uart_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UNUSED(context);

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        if(rx_buffer_index < LORA_RX_BUFFER_SIZE) {
            rx_buffer[rx_buffer_index++] = data;
        }
    }
}

static void lora_tester_scene_export_config_text_input_callback(void* context) {
    LoraTesterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraTesterCustomEventTextInputDone);
}

static void save_config_to_file(const char* filename, uint32_t current_baud_rate) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* file_path = furi_string_alloc();

    storage_common_mkdir(storage, CONFIG_FILE_DIRECTORY);

    furi_string_printf(
        file_path, "%s/%s%s", CONFIG_FILE_DIRECTORY, filename, CONFIG_FILE_EXTENSION);

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_format(stream, "[E220-900JP]\n");
        stream_write_format(stream, "own_address=%d\n", (rx_buffer[3] << 8) | rx_buffer[4]);
        stream_write_format(stream, "baud_rate=%lu\n", current_baud_rate);

        uint8_t air_data_rate = rx_buffer[5] & 0b00011111;
        uint16_t bw = 125;
        uint8_t sf = 5;
        if(air_data_rate & 0b00001)
            bw = 250;
        else if(air_data_rate & 0b00010)
            bw = 500;
        sf += (air_data_rate >> 2);
        stream_write_format(stream, "bw=%d\n", bw);
        stream_write_format(stream, "sf=%d\n", sf);

        uint8_t sub_packet_size = (rx_buffer[6] & 0b11000000) >> 6;
        const uint16_t sub_packet_sizes[] = {200, 128, 64, 32};
        stream_write_format(stream, "subpacket_size=%d\n", sub_packet_sizes[sub_packet_size]);

        stream_write_format(
            stream, "rssi_ambient_noise_flag=%d\n", (rx_buffer[6] & 0b00100000) >> 5);

        uint8_t transmit_power = rx_buffer[6] & 0b00000011;
        const int8_t transmit_powers[] = {22, 17, 13, 0};
        stream_write_format(stream, "transmitting_power=%d\n", transmit_powers[transmit_power]);

        stream_write_format(stream, "own_channel=%d\n", rx_buffer[7]);
        stream_write_format(stream, "rssi_byte_flag=%d\n", (rx_buffer[8] & 0b10000000) >> 7);
        stream_write_format(
            stream, "transmission_method_type=%d\n", ((rx_buffer[8] & 0b01000000) >> 6) + 1);

        uint8_t wor_cycle = rx_buffer[8] & 0b00000111;
        const uint16_t wor_cycles[] = {500, 1000, 1500, 2000, 2500, 3000, 3500, 4000};
        stream_write_format(stream, "wor_cycle=%d\n", wor_cycles[wor_cycle]);

        stream_write_format(stream, "encryption_key=%d\n", (rx_buffer[9] << 8) | rx_buffer[10]);

        FURI_LOG_I("LoRaTester", "Config saved to %s", furi_string_get_cstr(file_path));
    } else {
        FURI_LOG_E("LoRaTester", "Failed to open file for writing");
    }

    stream_free(stream);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
}

void lora_tester_scene_export_config_on_enter(void* context) {
    LoraTesterApp* app = context;
    furi_assert(app != NULL);

    FURI_LOG_I("LoRaTester", "Entering export config scene");

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    furi_string_reset(app->text_box_store);

    original_mode = app->current_mode;

    furi_string_printf(app->text_box_store, "Trying to find LoRa Module...\n");
    furi_string_cat_printf(
        app->text_box_store, "Original mode: %s\n", lora_mode_names[original_mode]);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextBox);

    lora_tester_set_mode(app, LoRaMode_Config);

    furi_delay_ms(100);

    uint32_t current_baud_rate = get_current_baud_rate(app);
    FuriHalSerialHandle* serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(serial_handle == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
        furi_string_printf(app->text_box_store, "Error: Failed to acquire serial handle");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        return;
    }

    furi_hal_serial_init(serial_handle, current_baud_rate);
    furi_hal_serial_enable_direction(
        serial_handle, FuriHalSerialDirectionRx | FuriHalSerialDirectionTx);

    rx_buffer_index = 0;
    furi_hal_serial_async_rx_start(serial_handle, lora_tester_uart_rx_callback, NULL, true);

    uint8_t command[] = {0xC1, 0x00, 0x08};
    furi_hal_serial_tx(serial_handle, command, sizeof(command));
    furi_hal_serial_tx_wait_complete(serial_handle);

    FURI_LOG_I("LoRaTester", "Command sent: C1 00 08 at %lu baud", current_baud_rate);

    uint32_t start_time = furi_get_tick();
    while(furi_get_tick() - start_time < LORA_SEARCH_TIMEOUT) {
        if(rx_buffer_index >= 11) {
            break;
        }
        furi_delay_ms(100);
    }

    furi_hal_serial_async_rx_stop(serial_handle);
    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);

    if(rx_buffer_index >= 11) {
        FURI_LOG_I(
            "LoRaTester",
            "Received %d bytes from LoRa module at %lu baud",
            rx_buffer_index,
            current_baud_rate);
        uart_text_input_set_header_text(app->text_input, "Enter config file name");
        uart_text_input_set_result_callback(
            app->text_input,
            lora_tester_scene_export_config_text_input_callback,
            app,
            app->text_input_store,
            TEXT_INPUT_STORE_SIZE,
            false);

        view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextInput);
    } else {
        FURI_LOG_E(
            "LoRaTester",
            "Failed to receive data from LoRa module at %lu baud",
            current_baud_rate);
        furi_string_printf(app->text_box_store, "Failed to find LoRa module within 3 seconds");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    }

    FURI_LOG_I("LoRaTester", "Export config scene setup complete");
}

void lora_tester_scene_export_config_on_exit(void* context) {
    LoraTesterApp* app = context;

    FURI_LOG_D("LoRaTester", "Exiting export config scene");

    FURI_LOG_D("LoRaTester", "Restoring original LoRa mode: %s", lora_mode_names[original_mode]);
    lora_tester_set_mode(app, original_mode);

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}

bool lora_tester_scene_export_config_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraTesterCustomEventTextInputDone) {
            uint32_t current_baud_rate = get_current_baud_rate(app);
            save_config_to_file(app->text_input_store, current_baud_rate);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoraTesterSceneStart);
        consumed = true;
    }

    return consumed;
}