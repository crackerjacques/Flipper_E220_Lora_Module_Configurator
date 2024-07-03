#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/path.h>
#include <dialogs/dialogs.h>
#include <gui/modules/file_browser.h>
#include "lora_tester_icons.h"
#include "../lora_config_binary_convert.h"

#define CONFIG_FILE_DIRECTORY "/ext/LoRa_Setting"
#define CONFIG_FILE_EXTENSION ".ini"
#define LORA_TX_BUFFER_SIZE 11
#define LORA_APPLY_TIMEOUT 3000

static uint8_t tx_buffer[LORA_TX_BUFFER_SIZE];
static LoRaMode original_mode;

static uint32_t get_current_baud_rate(LoraTesterApp* app) {
    return app->baud_rate;
}

static bool parse_config_file(Stream* stream, LoRaConfig* config) {
    FuriString* line = furi_string_alloc();
    bool success = true;

    //default values
    memset(config, 0, sizeof(LoRaConfig));
    config->baud_rate = 9600;
    config->bw = 125;
    config->sf = 7;
    config->subpacket_size = 200;
    config->transmitting_power = 13;
    config->wor_cycle = 500;

    while(stream_read_line(stream, line)) {
        if(furi_string_start_with_str(line, "own_address=")) {
            config->own_address = atoi(furi_string_get_cstr(line) + 12);
        } else if(furi_string_start_with_str(line, "baud_rate=")) {
            config->baud_rate = atoi(furi_string_get_cstr(line) + 10);
        } else if(furi_string_start_with_str(line, "bw=")) {
            config->bw = atoi(furi_string_get_cstr(line) + 3);
        } else if(furi_string_start_with_str(line, "sf=")) {
            config->sf = atoi(furi_string_get_cstr(line) + 3);
        } else if(furi_string_start_with_str(line, "subpacket_size=")) {
            config->subpacket_size = atoi(furi_string_get_cstr(line) + 15);
        } else if(furi_string_start_with_str(line, "rssi_ambient_noise_flag=")) {
            config->rssi_ambient_noise_flag = atoi(furi_string_get_cstr(line) + 24);
        } else if(furi_string_start_with_str(line, "transmitting_power=")) {
            config->transmitting_power = atoi(furi_string_get_cstr(line) + 19);
        } else if(furi_string_start_with_str(line, "own_channel=")) {
            config->own_channel = atoi(furi_string_get_cstr(line) + 12);
        } else if(furi_string_start_with_str(line, "rssi_byte_flag=")) {
            config->rssi_byte_flag = atoi(furi_string_get_cstr(line) + 15);
        } else if(furi_string_start_with_str(line, "transmission_method_type=")) {
            config->transmission_method_type = atoi(furi_string_get_cstr(line) + 26);
        } else if(furi_string_start_with_str(line, "wor_cycle=")) {
            config->wor_cycle = atoi(furi_string_get_cstr(line) + 10);
        } else if(furi_string_start_with_str(line, "encryption_key=")) {
            config->encryption_key = atoi(furi_string_get_cstr(line) + 15);
        }
    }

    furi_string_free(line);
    return success;
}

static bool load_and_apply_config(LoraTesterApp* app, const char* filename) {
    FURI_LOG_I("LoRaTester", "Attempting to load and apply config: %s", filename);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FuriString* file_path = furi_string_alloc();
    bool success = false;
    LoRaConfig config;

    furi_string_printf(
        file_path, "%s/%s%s", CONFIG_FILE_DIRECTORY, filename, CONFIG_FILE_EXTENSION);

    FURI_LOG_D("LoRaTester", "Opening file: %s", furi_string_get_cstr(file_path));
    if(file_stream_open(stream, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_D("LoRaTester", "File opened successfully, parsing config");
        if(parse_config_file(stream, &config)) {
            FURI_LOG_D("LoRaTester", "Config parsed successfully, setting LoRa mode");
            original_mode = app->current_mode;
            furi_delay_ms(100);
            lora_tester_set_mode(app, LoRaMode_Config);
            furi_delay_ms(100);

            uint32_t current_baud_rate = get_current_baud_rate(app);
            FURI_LOG_D("LoRaTester", "Acquiring serial handle");
            FuriHalSerialHandle* serial_handle =
                furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(serial_handle != NULL) {
                FURI_LOG_D(
                    "LoRaTester",
                    "Serial handle acquired, initializing with baud rate: %lu",
                    current_baud_rate);
                furi_hal_serial_init(serial_handle, current_baud_rate);
                furi_hal_serial_enable_direction(
                    serial_handle, FuriHalSerialDirectionRx | FuriHalSerialDirectionTx);

                char hex_string[33];
                if(lora_config_to_hex_string(&config, hex_string, sizeof(hex_string))) {
                    FURI_LOG_D("LoRaTester", "Config converted to hex: %s", hex_string);

                    unsigned int byte;
                    int index = 0;
                    char* hex_ptr = hex_string;
                    while(sscanf(hex_ptr, "%2x", &byte) == 1 && index < LORA_TX_BUFFER_SIZE) {
                        tx_buffer[index++] = (uint8_t)byte;
                        hex_ptr += 2;
                        while(*hex_ptr == ' ') hex_ptr++;
                    }

                    FURI_LOG_D(
                        "LoRaTester",
                        "Sending config to LoRa module at %lu baud",
                        current_baud_rate);
                    furi_hal_serial_tx(serial_handle, tx_buffer, LORA_TX_BUFFER_SIZE);
                    furi_hal_serial_tx_wait_complete(serial_handle);
                    success = true;
                    FURI_LOG_I(
                        "LoRaTester",
                        "Config applied successfully at %lu baud",
                        current_baud_rate);
                } else {
                    FURI_LOG_E("LoRaTester", "Failed to convert config to hex");
                }

                FURI_LOG_D("LoRaTester", "Config sent, deinitializing serial");
                furi_hal_serial_deinit(serial_handle);
                furi_hal_serial_control_release(serial_handle);
            } else {
                FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
            }

            FURI_LOG_D(
                "LoRaTester", "Restoring original LoRa mode: %s", lora_mode_names[original_mode]);
            furi_delay_ms(100);
            lora_tester_set_mode(app, original_mode);
            furi_delay_ms(100);
        } else {
            FURI_LOG_E("LoRaTester", "Failed to parse config file");
        }
    } else {
        FURI_LOG_E("LoRaTester", "Failed to open file for reading");
    }

    stream_free(stream);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);

    return success;
}

static bool display_config_content(LoraTesterApp* app, const char* filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FuriString* file_path = furi_string_alloc();
    FuriString* line = furi_string_alloc();

    furi_string_printf(
        file_path, "%s/%s%s", CONFIG_FILE_DIRECTORY, filename, CONFIG_FILE_EXTENSION);

    furi_string_reset(app->text_box_store);
    furi_string_printf(app->text_box_store, "Config File: %s\n\n", filename);

    bool success = false;

    if(file_stream_open(stream, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(furi_string_size(line) > 0) {
                furi_string_cat_printf(app->text_box_store, "%s\n", furi_string_get_cstr(line));
            }
        }
        success = true;
    } else {
        furi_string_cat_printf(app->text_box_store, "Failed to open config file for reading\n");
    }

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));

    stream_free(stream);
    furi_string_free(file_path);
    furi_string_free(line);
    furi_record_close(RECORD_STORAGE);

    return success;
}

void lora_tester_scene_config_load_on_enter(void* context) {
    LoraTesterApp* app = context;
    FURI_LOG_I("LoRaTester", "Entering config load scene");

    original_mode = app->current_mode;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, CONFIG_FILE_EXTENSION, &I_lora_10px);
    browser_options.base_path = CONFIG_FILE_DIRECTORY;

    bool result =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    if(result) {
        FURI_LOG_D("LoRaTester", "File selected: %s", furi_string_get_cstr(app->file_path));
        FuriString* filename = furi_string_alloc();
        path_extract_filename(app->file_path, filename, true);

        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Config file selected", 64, 0, AlignCenter, AlignTop);
        dialog_message_set_text(
            message, furi_string_get_cstr(filename), 64, 32, AlignCenter, AlignCenter);
        dialog_message_set_buttons(message, "View", "Apply", "Back");

        DialogMessageButton choice = dialog_message_show(app->dialogs, message);
        dialog_message_free(message);

        switch(choice) {
        case DialogMessageButtonLeft:
            if(display_config_content(app, furi_string_get_cstr(filename))) {
                view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextBox);
            } else {
                dialog_message_show_storage_error(app->dialogs, "Failed to display config file");
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, LoraTesterSceneStart);
            }
            break;
        case DialogMessageButtonCenter:
            if(load_and_apply_config(app, furi_string_get_cstr(filename))) {
                DialogMessage* success_message = dialog_message_alloc();
                dialog_message_set_header(
                    success_message, "Success", 64, 0, AlignCenter, AlignTop);
                dialog_message_set_text(
                    success_message,
                    "Config applied successfully",
                    64,
                    32,
                    AlignCenter,
                    AlignCenter);
                dialog_message_set_buttons(success_message, NULL, "OK", NULL);
                dialog_message_show(app->dialogs, success_message);
                dialog_message_free(success_message);
            } else {
                dialog_message_show_storage_error(app->dialogs, "Failed to apply config");
            }
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, LoraTesterSceneStart);
            break;
        case DialogMessageButtonRight:
        case DialogMessageButtonBack:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, LoraTesterSceneStart);
            break;
        }

        furi_string_free(filename);
    } else {
        FURI_LOG_D("LoRaTester", "File selection cancelled");
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoraTesterSceneStart);
    }

    FURI_LOG_I("LoRaTester", "Config load scene setup complete");
}

bool lora_tester_scene_config_load_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraTesterCustomEventTextInputDone) {
            FuriString* filename = furi_string_alloc();
            path_extract_filename(app->file_path, filename, true);

            if(load_and_apply_config(app, furi_string_get_cstr(filename))) {
                DialogMessage* success_message = dialog_message_alloc();
                dialog_message_set_header(
                    success_message, "Success", 64, 0, AlignCenter, AlignTop);
                dialog_message_set_text(
                    success_message,
                    "Config applied successfully",
                    64,
                    32,
                    AlignCenter,
                    AlignCenter);
                dialog_message_set_buttons(success_message, NULL, "OK", NULL);
                dialog_message_show(app->dialogs, success_message);
                dialog_message_free(success_message);
            } else {
                dialog_message_show_storage_error(app->dialogs, "Failed to apply config");
            }

            furi_string_free(filename);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, LoraTesterSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("LoRaTester", "Back event received, switching to start scene");
        lora_tester_set_mode(app, original_mode);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoraTesterSceneStart);
        consumed = true;
    }

    return consumed;
}

void lora_tester_scene_config_load_on_exit(void* context) {
    LoraTesterApp* app = context;
    FURI_LOG_I("LoRaTester", "Exiting config load scene");

    lora_tester_set_mode(app, original_mode);

    furi_string_reset(app->file_path);
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}