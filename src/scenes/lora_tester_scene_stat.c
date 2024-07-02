#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <gui/elements.h>

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

static void display_lora_stats(LoraTesterApp* app) {
    furi_string_reset(app->text_box_store);

    if(rx_buffer_index >= 11) {
        uint16_t address = (rx_buffer[3] << 8) | rx_buffer[4];
        furi_string_cat_printf(app->text_box_store, "Address: 0x%04X\n", address);

        uint8_t uart_rate = (rx_buffer[5] & 0b11100000) >> 5;
        const char* uart_rates[] = {
            "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"};
        furi_string_cat_printf(app->text_box_store, "UART: %sbps\n", uart_rates[uart_rate]);

        uint8_t air_data_rate = rx_buffer[5] & 0b00011111;
        const char* air_data_rate_settings[] = {
            "15.625kbps SF5 BW125",
            "31.25kbps SF5 BW250",
            "62.5kbps SF5 BW500",
            "9.375kbps SF6 BW125",
            "18.75kbps SF6 BW250",
            "37.5kbps SF6 BW500",
            "5.469kbps SF7 BW125",
            "10.938kbps SF7 BW250",
            "21.875kbps SF7 BW500",
            "3.125kbps SF8 BW125",
            "6.25kbps SF8 BW250",
            "12.5kbps SF8 BW500",
            "1.758kbps SF9 BW125",
            "3.516kbps SF9 BW250",
            "7.031kbps SF9 BW500",
            "",
            "1.953kbps SF10 BW250",
            "3.906kbps SF10 BW500",
            "",
            "",
            "2.148kbps SF11 BW500"};
        if(air_data_rate < sizeof(air_data_rate_settings) / sizeof(air_data_rate_settings[0]) &&
           air_data_rate_settings[air_data_rate][0] != '\0') {
            furi_string_cat_printf(
                app->text_box_store, "Air Data Rate: %s\n", air_data_rate_settings[air_data_rate]);
        } else {
            furi_string_cat_printf(
                app->text_box_store, "Air Data Rate: Unknown (%d)\n", air_data_rate);
        }

        uint8_t sub_packet_size = (rx_buffer[6] & 0b11000000) >> 6;
        const uint16_t sub_packet_sizes[] = {200, 128, 64, 32};
        furi_string_cat_printf(
            app->text_box_store, "Sub Packet Size: %d bytes\n", sub_packet_sizes[sub_packet_size]);

        uint8_t rssi_ambient = (rx_buffer[6] & 0b00100000) >> 5;
        furi_string_cat_printf(
            app->text_box_store, "RSSI Ambient: %s\n", rssi_ambient ? "Enable" : "Disable");

        uint8_t transmit_power = rx_buffer[6] & 0b00000011;
        const int8_t transmit_powers[] = {22, 17, 13, 0};
        furi_string_cat_printf(
            app->text_box_store, "Tx Power: %d dBm\n", transmit_powers[transmit_power]);

        uint8_t channel = rx_buffer[7];
        furi_string_cat_printf(app->text_box_store, "Channel: %d\n", channel);

        uint8_t rssi_byte = (rx_buffer[8] & 0b10000000) >> 7;
        furi_string_cat_printf(
            app->text_box_store, "RSSI Byte: %s\n", rssi_byte ? "Enable" : "Disable");

        uint8_t transmission_method = (rx_buffer[8] & 0b01000000) >> 6;
        furi_string_cat_printf(
            app->text_box_store,
            "Transmission Method: %s\n",
            transmission_method ? "Fixed transmission" : "Transparent transmission");

        uint8_t wor_cycle = rx_buffer[8] & 0b00000111;
        const char* wor_cycles[] = {
            "500ms", "1000ms", "1500ms", "2000ms", "2500ms", "3000ms", "3500ms", "4000ms"};
        furi_string_cat_printf(app->text_box_store, "WOR Cycle: %s\n", wor_cycles[wor_cycle]);

        uint16_t encryption_key = (rx_buffer[9] << 8) | rx_buffer[10];
        furi_string_cat_printf(app->text_box_store, "Encryption Key: 0x%04X\n", encryption_key);
    } else {
        furi_string_cat_printf(app->text_box_store, "Invalid response from LoRa module\n");
    }

    // Add AUX pin status
    bool aux_state = furi_hal_gpio_read(&gpio_ext_pa4);
    furi_string_cat_printf(app->text_box_store, "Aux: %s\n", aux_state ? "High" : "Low");

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

void lora_tester_scene_stat_on_enter(void* context) {
    LoraTesterApp* app = context;
    furi_assert(app != NULL);
    FURI_LOG_I("LoRaTester", "Entering stat scene");

    furi_hal_gpio_init(&gpio_ext_pa4, GpioModeInput, GpioPullNo, GpioSpeedLow);
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    furi_string_reset(app->text_box_store);

    original_mode = app->current_mode;

    furi_string_printf(app->text_box_store, "Trying to find LoRa Module...\n");
    furi_string_cat_printf(
        app->text_box_store, "Original mode: %s\n", lora_mode_names[original_mode]);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextBox);

    lora_tester_set_mode(app, LoRaMode_Normal);

    furi_delay_ms(100);

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
        display_lora_stats(app);
    } else {
        FURI_LOG_E(
            "LoRaTester",
            "Failed to receive data from LoRa module at %lu baud",
            current_baud_rate);
        furi_string_printf(app->text_box_store, "Failed to find LoRa module within 3 seconds\n");

        bool aux_state = furi_hal_gpio_read(&gpio_ext_pa4);
        furi_string_cat_printf(app->text_box_store, "Aux: %s\n", aux_state ? "High" : "Low");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    }

    FURI_LOG_I("LoRaTester", "Stat scene setup complete");
}

void lora_tester_scene_stat_on_exit(void* context) {
    LoraTesterApp* app = context;

    FURI_LOG_D("LoRaTester", "Exiting stat scene");

    FURI_LOG_D("LoRaTester", "Restoring original LoRa mode: %s", lora_mode_names[original_mode]);
    lora_tester_set_mode(app, original_mode);

    furi_hal_gpio_init(&gpio_ext_pa4, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_delay_ms(100);

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}

bool lora_tester_scene_stat_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("LoRaTester", "Back button pressed in stat scene");
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoraTesterSceneStart);
        consumed = true;
    }

    return consumed;
}