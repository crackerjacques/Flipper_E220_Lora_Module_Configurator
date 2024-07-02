#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include "../lora_config_hex_convert.h"
#include "lora_tester_icons.h"

#define LORA_RX_BUFFER_SIZE 256
#define LORA_SEARCH_TIMEOUT 3000

static uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];
static size_t rx_buffer_index = 0;
static LoRaMode original_mode;

static void update_channel_range(LoraTesterApp* app, uint8_t air_data_rate_index);
static void update_frequency_display(LoraTesterApp* app, uint8_t channel, uint8_t bw);

static const char* uart_rates[] =
    {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"};
static const char* sub_packet_sizes[] = {"200", "128", "64", "32"};
static const char* air_data_rates[] = {
    "15.6k, SF:5,125kHz",
    "9.37k, SF:6,125kHz",
    "5.47k, SF:7,125kHz",
    "3.12k, SF:8,125kHz",
    "1.76k, SF:9,125kHz",
    "31.25k, SF:5,250kHz",
    "18.8k, SF:6,250kHz",
    "11k, SF:7,250kHz",
    "6.25k, SF:8,250kHz",
    "3.51k, SF:9,250kHz",
    "1.95k, SF:10,250kHz",
    "62.5k, SF:5,500kHz",
    "37.5k, SF:6,500kHz",
    "21.9k, SF:7,500kHz",
    "12.5k, SF:8,500kHz",
    "7.03k, SF:9,500kHz",
    "3.9k, SF:10,500kHz",
    "2.14k, SF:11,500kHz"};
static const char* rssi_options[] = {"Disable", "Enable"};
static const char* tx_powers[] = {"22", "17", "13", "0"};
static const char* transmission_methods[] = {"Transparent", "Fixed"};
static const char* wor_cycles[] = {"500", "1000", "1500", "2000", "2500", "3000", "3500", "4000"};

static uint32_t get_current_baud_rate(LoraTesterApp* app) {
    return app->baud_rate;
}

static uint8_t get_max_channel(uint8_t bw) {
    switch(bw) {
    case 0: // BW125kHz
        return 37;
    case 1: // BW250kHz
        return 36;
    case 2: // BW500kHz
        return 30;
    default:
        return 0;
    }
}

static void update_frequency_display(LoraTesterApp* app, uint8_t channel, uint8_t bw) {
    if(app == NULL || app->config_items == NULL ||
       app->config_items->items[ConfigureItemChannel] == NULL) {
        FURI_LOG_E("LoRaTester", "Invalid app or config items in update_frequency_display");
        return;
    }

    float base_freq;
    switch(bw) {
    case 0: // BW125kHz
        base_freq = 920.6f;
        break;
    case 1: // BW250kHz
        base_freq = 920.7f;
        break;
    case 2: // BW500kHz
        base_freq = 920.8f;
        break;
    default:
        FURI_LOG_E("LoRaTester", "Invalid bandwidth value: %d", bw);
        return;
    }

    float frequency = base_freq + channel * 0.2f;

    char freq_str[32];
    int ret = snprintf(freq_str, sizeof(freq_str), "%d (%.1f MHz)", channel, (double)frequency);
    if(ret < 0 || ret >= (int)sizeof(freq_str)) {
        FURI_LOG_E("LoRaTester", "Error formatting frequency string");
        return;
    }

    VariableItem* channel_item = app->config_items->items[ConfigureItemChannel];
    if(channel_item == NULL) {
        FURI_LOG_E("LoRaTester", "Channel item is NULL in update_frequency_display");
        return;
    }

    variable_item_set_current_value_text(channel_item, freq_str);
    FURI_LOG_I("LoRaTester", "Updated frequency display: %s", freq_str);
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

static void get_current_config(LoraTesterApp* app, LoRaConfig* config) {
    // 初期値を設定
    memset(config, 0, sizeof(LoRaConfig));
    config->baud_rate = 9600;
    config->bw = 125;
    config->sf = 7;
    config->subpacket_size = 200;
    config->transmitting_power = 13;
    config->wor_cycle = 2000;

    for(int i = 0; i < ConfigureItemCount; i++) {
        VariableItem* item = app->config_items->items[i];
        uint8_t index = variable_item_get_current_value_index(item);

        switch(i) {
        case ConfigureItemAddress:
            config->own_address = index;
            break;
        case ConfigureItemUARTRate:
            config->baud_rate = atoi(uart_rates[index]);
            break;
        case ConfigureItemAirDataRate:
            if(index < 5) {
                config->bw = 125;
                config->sf = index + 5;
            } else if(index < 11) {
                config->bw = 250;
                config->sf = (index - 5) + 5;
            } else {
                config->bw = 500;
                config->sf = (index - 11) + 5;
            }
            break;
        case ConfigureItemSubPacketSize:
            config->subpacket_size = atoi(sub_packet_sizes[index]);
            break;
        case ConfigureItemRSSIAmbient:
            config->rssi_ambient_noise_flag = index;
            break;
        case ConfigureItemTxPower:
            switch(index) {
            case 0:
                config->transmitting_power = 22;
                break;
            case 1:
                config->transmitting_power = 17;
                break;
            case 2:
                config->transmitting_power = 13;
                break;
            case 3:
                config->transmitting_power = 0;
                break;
            default:
                config->transmitting_power = 13;
                break;
            }
            break;
        case ConfigureItemChannel:
            config->own_channel = index;
            break;
        case ConfigureItemRSSIByte:
            config->rssi_byte_flag = index;
            break;
        case ConfigureItemTransmissionMethod:
            config->transmission_method_type = index;
            break;
        case ConfigureItemWORCycle:
            config->wor_cycle = (index + 1) * 500;
            break;
        case ConfigureItemEncryptionKey:
            config->encryption_key = index;
            break;
        default:
            break;
        }
    }
}

static void show_saved_popup(LoraTesterApp* app) {
    popup_reset(app->popup);

    popup_set_header(app->popup, "SAVED!!", 24, 10, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 22, 6, &I_DolphinNice_96x59);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewPopup);
    furi_delay_ms(1000);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewVarItemList);
}

static void send_config_to_lora_module(LoraTesterApp* app) {
    LoRaConfig config;
    get_current_config(app, &config);

    uint8_t command[11] = {0xC0, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Address
    command[3] = (config.own_address >> 8) & 0xFF;
    command[4] = config.own_address & 0xFF;

    // UART Rate and Air Data Rate
    uint8_t uart_rate_bits = 0;
    switch(config.baud_rate) {
    case 1200:
        uart_rate_bits = 0;
        break;
    case 2400:
        uart_rate_bits = 1;
        break;
    case 4800:
        uart_rate_bits = 2;
        break;
    case 9600:
        uart_rate_bits = 3;
        break;
    case 19200:
        uart_rate_bits = 4;
        break;
    case 38400:
        uart_rate_bits = 5;
        break;
    case 57600:
        uart_rate_bits = 6;
        break;
    case 115200:
        uart_rate_bits = 7;
        break;
    default:
        uart_rate_bits = 3;
        break; // Default to 9600
    }

    uint8_t air_data_rate_bits = 0;
    if(config.bw == 125) {
        air_data_rate_bits = (config.sf - 5) * 4;
    } else if(config.bw == 250) {
        air_data_rate_bits = 1 + (config.sf - 5) * 4;
    } else if(config.bw == 500) {
        air_data_rate_bits = 2 + (config.sf - 5) * 4;
    }

    command[5] = (uart_rate_bits << 5) | air_data_rate_bits;

    // Sub Packet Size, RSSI Ambient, and Tx Power
    uint8_t sub_packet_bits = 0;
    switch(config.subpacket_size) {
    case 200:
        sub_packet_bits = 0;
        break;
    case 128:
        sub_packet_bits = 1;
        break;
    case 64:
        sub_packet_bits = 2;
        break;
    case 32:
        sub_packet_bits = 3;
        break;
    default:
        sub_packet_bits = 0;
        break;
    }

    uint8_t tx_power_bits = 0;
    switch(config.transmitting_power) {
    case 22:
        tx_power_bits = 0;
        break;
    case 17:
        tx_power_bits = 1;
        break;
    case 13:
        tx_power_bits = 2;
        break;
    case 0:
        tx_power_bits = 3;
        break;
    default:
        tx_power_bits = 2;
        break; // Default to 13dBm
    }

    command[6] = (sub_packet_bits << 6) | (config.rssi_ambient_noise_flag << 5) | tx_power_bits;

    // Channel
    command[7] = config.own_channel;

    // RSSI Byte, Transmission Method, and WOR Cycle
    uint8_t wor_cycle_bits = (config.wor_cycle / 500) - 1;
    if(wor_cycle_bits > 7) wor_cycle_bits = 7;

    command[8] = (config.rssi_byte_flag << 7) | (config.transmission_method_type << 6) |
                 wor_cycle_bits;

    // Encryption Key
    command[9] = (config.encryption_key >> 8) & 0xFF;
    command[10] = config.encryption_key & 0xFF;

    uint32_t current_baud_rate = get_current_baud_rate(app);
    FuriHalSerialHandle* serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(serial_handle == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
        return;
    }

    furi_hal_serial_init(serial_handle, current_baud_rate);
    furi_hal_serial_enable_direction(
        serial_handle, FuriHalSerialDirectionRx | FuriHalSerialDirectionTx);

    furi_hal_serial_tx(serial_handle, command, sizeof(command));
    furi_hal_serial_tx_wait_complete(serial_handle);

    furi_delay_ms(100);

    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);

    FURI_LOG_I("LoRaTester", "Config sent to LoRa module at %lu baud", current_baud_rate);
    show_saved_popup(app);
}

static void configure_item_change_callback(VariableItem* item) {
    LoraTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    FURI_LOG_D("LoRaTester", "Item changed, index: %d", index);

    for(int i = 0; i < ConfigureItemCount; i++) {
        if(app->config_items->items[i] == item) {
            switch(i) {
            case ConfigureItemAddress: {
                char value_str[32];
                snprintf(value_str, sizeof(value_str), "0x%04X", index);
                variable_item_set_current_value_text(item, value_str);
                FURI_LOG_D("LoRaTester", "Address changed to: %s", value_str);
            } break;
            case ConfigureItemUARTRate:
                variable_item_set_current_value_text(item, uart_rates[index]);
                FURI_LOG_D("LoRaTester", "UART Rate changed to: %s", uart_rates[index]);
                break;
            case ConfigureItemAirDataRate:
                variable_item_set_current_value_text(item, air_data_rates[index]);
                FURI_LOG_D("LoRaTester", "Air Data Rate changed to: %s", air_data_rates[index]);
                update_channel_range(app, index);
                break;
            case ConfigureItemSubPacketSize:
                variable_item_set_current_value_text(item, sub_packet_sizes[index]);
                FURI_LOG_D(
                    "LoRaTester", "Sub Packet Size changed to: %s", sub_packet_sizes[index]);
                break;
            case ConfigureItemRSSIAmbient:
                variable_item_set_current_value_text(item, rssi_options[index]);
                FURI_LOG_D("LoRaTester", "RSSI Ambient changed to: %s", rssi_options[index]);
                break;
            case ConfigureItemTxPower:
                variable_item_set_current_value_text(item, tx_powers[index]);
                FURI_LOG_D("LoRaTester", "Tx Power changed to: %s", tx_powers[index]);
                break;
            case ConfigureItemChannel: {
                char channel_str[8];
                snprintf(channel_str, sizeof(channel_str), "%d", index);
                variable_item_set_current_value_text(item, channel_str);
                FURI_LOG_D("LoRaTester", "Channel changed to: %d", index);

                // Update frequency display
                VariableItem* air_data_rate_item =
                    app->config_items->items[ConfigureItemAirDataRate];
                if(air_data_rate_item) {
                    uint8_t air_data_rate_index =
                        variable_item_get_current_value_index(air_data_rate_item);
                    uint8_t bw = air_data_rate_index / 5;
                    update_frequency_display(app, index, bw);
                }
            } break;
            case ConfigureItemRSSIByte:
                variable_item_set_current_value_text(item, rssi_options[index]);
                FURI_LOG_D("LoRaTester", "RSSI Byte changed to: %s", rssi_options[index]);
                break;
            case ConfigureItemTransmissionMethod:
                variable_item_set_current_value_text(item, transmission_methods[index]);
                FURI_LOG_D("LoRaTester", "Tx Method changed to: %s", transmission_methods[index]);
                break;
            case ConfigureItemWORCycle:
                variable_item_set_current_value_text(item, wor_cycles[index]);
                FURI_LOG_D("LoRaTester", "WOR Cycle changed to: %s", wor_cycles[index]);
                break;
            case ConfigureItemEncryptionKey: {
                char key_str[16];
                snprintf(key_str, sizeof(key_str), "0x%04X", index);
                variable_item_set_current_value_text(item, key_str);
                FURI_LOG_D("LoRaTester", "Encryption Key changed to: %s", key_str);
            } break;
            case ConfigureItemSave:
                FURI_LOG_I("LoRaTester", "Save button pressed");
                send_config_to_lora_module(app);
                break;
            default:
                FURI_LOG_W("LoRaTester", "Unknown item changed, index: %d", i);
                break;
            }
            break;
        }
    }
}

// Channel の範囲を更新する関数
static void update_channel_range(LoraTesterApp* app, uint8_t air_data_rate_index) {
    uint8_t bw = air_data_rate_index / 5; // Approximate BW from air data rate index
    uint8_t max_channel = get_max_channel(bw);

    VariableItem* channel_item = app->config_items->items[ConfigureItemChannel];
    if(channel_item) {
        uint8_t current_channel = variable_item_get_current_value_index(channel_item);
        variable_item_set_values_count(channel_item, max_channel + 1);

        if(current_channel > max_channel) {
            variable_item_set_current_value_index(channel_item, max_channel);
            update_frequency_display(app, max_channel, bw);
        } else {
            update_frequency_display(app, current_channel, bw);
        }
        FURI_LOG_D("LoRaTester", "Channel range updated, max: %d", max_channel);
    }
}

static void load_configure_data(LoraTesterApp* app) {
    uint32_t current_baud_rate = get_current_baud_rate(app);
    FuriHalSerialHandle* serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(serial_handle == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to acquire serial handle");
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
        FURI_LOG_I("LoRaTester", "Received %d bytes from LoRa module", rx_buffer_index);
        VariableItemList* var_item_list = app->var_item_list;
        VariableItem* item;
        char value_str[32];

        // Address
        uint16_t address = (rx_buffer[3] << 8) | rx_buffer[4];
        snprintf(value_str, sizeof(value_str), "0x%04X", address);
        item = variable_item_list_add(
            var_item_list, "Address", 1, configure_item_change_callback, app);
        app->config_items->items[ConfigureItemAddress] = item;
        variable_item_set_current_value_text(item, value_str);
        FURI_LOG_D("LoRaTester", "Address set to: %s", value_str);

        // UART Rate
        uint8_t uart_rate = (rx_buffer[5] & 0b11100000) >> 5;
        item = variable_item_list_add(
            var_item_list, "UART Rate", COUNT_OF(uart_rates), configure_item_change_callback, app);
        app->config_items->items[ConfigureItemUARTRate] = item;
        variable_item_set_current_value_index(item, uart_rate);
        variable_item_set_current_value_text(item, uart_rates[uart_rate]);
        FURI_LOG_D("LoRaTester", "UART Rate set to: %s", uart_rates[uart_rate]);

        // Air Data Rate
        uint8_t air_data_rate = rx_buffer[5] & 0b00011111;
        item = variable_item_list_add(
            var_item_list, "ADR", COUNT_OF(air_data_rates), configure_item_change_callback, app);
        app->config_items->items[ConfigureItemAirDataRate] = item;
        variable_item_set_current_value_index(item, air_data_rate);
        variable_item_set_current_value_text(item, air_data_rates[air_data_rate]);
        FURI_LOG_D("LoRaTester", "Air Data Rate set to: %s", air_data_rates[air_data_rate]);

        // Sub Packet Size
        uint8_t sub_packet_size = (rx_buffer[6] & 0b11000000) >> 6;
        item = variable_item_list_add(
            var_item_list,
            "Sub Packet Size",
            COUNT_OF(sub_packet_sizes),
            configure_item_change_callback,
            app);
        app->config_items->items[ConfigureItemSubPacketSize] = item;
        variable_item_set_current_value_index(item, sub_packet_size);
        variable_item_set_current_value_text(item, sub_packet_sizes[sub_packet_size]);
        FURI_LOG_D("LoRaTester", "Sub Packet Size set to: %s", sub_packet_sizes[sub_packet_size]);

        // RSSI Ambient Noise Flag
        uint8_t rssi_ambient = (rx_buffer[6] & 0b00100000) >> 5;
        item = variable_item_list_add(
            var_item_list,
            "RSSI Ambient",
            COUNT_OF(rssi_options),
            configure_item_change_callback,
            app);
        app->config_items->items[ConfigureItemRSSIAmbient] = item;
        variable_item_set_current_value_index(item, rssi_ambient);
        variable_item_set_current_value_text(item, rssi_options[rssi_ambient]);
        FURI_LOG_D("LoRaTester", "RSSI Ambient set to: %s", rssi_options[rssi_ambient]);

        // Transmitting Power
        uint8_t transmit_power = rx_buffer[6] & 0b00000011;
        item = variable_item_list_add(
            var_item_list, "Tx Power", COUNT_OF(tx_powers), configure_item_change_callback, app);
        app->config_items->items[ConfigureItemTxPower] = item;
        variable_item_set_current_value_index(item, transmit_power);
        variable_item_set_current_value_text(item, tx_powers[transmit_power]);
        FURI_LOG_D("LoRaTester", "Tx Power set to: %s", tx_powers[transmit_power]);

        // Channel
        uint8_t channel = rx_buffer[7];
        uint8_t bw = air_data_rate / 5; // Approximate BW from air data rate
        uint8_t max_channel = get_max_channel(bw);
        item = variable_item_list_add(
            var_item_list, "Channel", max_channel + 1, configure_item_change_callback, app);
        app->config_items->items[ConfigureItemChannel] = item;
        variable_item_set_current_value_index(item, channel);
        snprintf(value_str, sizeof(value_str), "%d", channel);
        variable_item_set_current_value_text(item, value_str);
        FURI_LOG_D("LoRaTester", "Channel set to: %d", channel);

        // Update frequency display
        update_frequency_display(app, channel, bw);

        // RSSI Byte
        uint8_t rssi_byte = (rx_buffer[8] & 0b10000000) >> 7;
        item = variable_item_list_add(
            var_item_list,
            "RSSI Byte",
            COUNT_OF(rssi_options),
            configure_item_change_callback,
            app);
        app->config_items->items[ConfigureItemRSSIByte] = item;
        variable_item_set_current_value_index(item, rssi_byte);
        variable_item_set_current_value_text(item, rssi_options[rssi_byte]);
        FURI_LOG_D("LoRaTester", "RSSI Byte set to: %s", rssi_options[rssi_byte]);

        // Transmission Method
        uint8_t transmission_method = (rx_buffer[8] & 0b01000000) >> 6;
        item = variable_item_list_add(
            var_item_list,
            "Tx Method",
            COUNT_OF(transmission_methods),
            configure_item_change_callback,
            app);
        app->config_items->items[ConfigureItemTransmissionMethod] = item;
        variable_item_set_current_value_index(item, transmission_method);
        variable_item_set_current_value_text(item, transmission_methods[transmission_method]);
        FURI_LOG_D(
            "LoRaTester", "Tx Method set to: %s", transmission_methods[transmission_method]);

        // WOR Cycle
        uint8_t wor_cycle = rx_buffer[8] & 0b00000111;
        item = variable_item_list_add(
            var_item_list, "WOR Cycle", COUNT_OF(wor_cycles), configure_item_change_callback, app);
        app->config_items->items[ConfigureItemWORCycle] = item;
        variable_item_set_current_value_index(item, wor_cycle);
        variable_item_set_current_value_text(item, wor_cycles[wor_cycle]);
        FURI_LOG_D("LoRaTester", "WOR Cycle set to: %s", wor_cycles[wor_cycle]);

        // Encryption Key
        uint16_t encryption_key = (rx_buffer[9] << 8) | rx_buffer[10];
        snprintf(value_str, sizeof(value_str), "0x%04X", encryption_key);
        item = variable_item_list_add(
            var_item_list, "Encryption Key", 1, configure_item_change_callback, app);
        app->config_items->items[ConfigureItemEncryptionKey] = item;
        variable_item_set_current_value_text(item, value_str);
        FURI_LOG_D("LoRaTester", "Encryption Key set to: %s", value_str);

        // Save button
        item =
            variable_item_list_add(var_item_list, "Save", 1, configure_item_change_callback, app);
        app->config_items->items[ConfigureItemSave] = item;
        variable_item_set_current_value_text(item, "Press OK");
    } else {
        FURI_LOG_E("LoRaTester", "Failed to receive data from LoRa module");
        variable_item_list_add(app->var_item_list, "Error", 0, NULL, NULL);
        variable_item_list_add(app->var_item_list, "Failed to load config", 0, NULL, NULL);
    }
}

void lora_tester_scene_configure_on_enter(void* context) {
    LoraTesterApp* app = context;
    furi_assert(app != NULL);

    FURI_LOG_I("LoRaTester", "Entering configure scene");

    // Initialize config_items
    app->config_items = malloc(sizeof(ConfigureItemsList));
    if(app->config_items == NULL) {
        FURI_LOG_E("LoRaTester", "Failed to allocate memory for config_items");
        return;
    }
    memset(app->config_items, 0, sizeof(ConfigureItemsList));

    original_mode = app->current_mode;
    lora_tester_set_mode(app, LoRaMode_Config);

    furi_delay_ms(100);

    variable_item_list_reset(app->var_item_list);
    load_configure_data(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewVarItemList);
}

bool lora_tester_scene_configure_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D(
            "LoRaTester", "Restoring original LoRa mode: %s", lora_mode_names[original_mode]);
        lora_tester_set_mode(app, original_mode);

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ConfigureItemSave) {
            send_config_to_lora_module(app);
            consumed = true;
        }
    }

    return consumed;
}

void lora_tester_scene_configure_on_exit(void* context) {
    LoraTesterApp* app = context;
    variable_item_list_reset(app->var_item_list);
    if(app->config_items != NULL) {
        free(app->config_items);
        app->config_items = NULL;
    }
}