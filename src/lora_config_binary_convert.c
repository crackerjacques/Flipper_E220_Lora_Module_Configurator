//I think other modules can be supported by rewriting the values here and in configure respectively.
// *However, it has not been tried yet.*

#include "lora_config_binary_convert.h"
#include <string.h>
#include <stdio.h>

static uint8_t get_uart_baud_rate_bits(uint32_t baud_rate) {
    switch(baud_rate) {
    case 1200:
        return 0b000;
    case 2400:
        return 0b001;
    case 4800:
        return 0b010;
    case 9600:
        return 0b011;
    case 19200:
        return 0b100;
    case 38400:
        return 0b101;
    case 57600:
        return 0b110;
    case 115200:
        return 0b111;
    default:
        return 0b011; //  9600bps
    }
}

static uint8_t get_air_data_rate_bits(uint16_t bw, uint8_t sf) {
    if(bw == 125) {
        switch(sf) {
        case 5:
            return 0b00000;
        case 6:
            return 0b00100;
        case 7:
            return 0b01000;
        case 8:
            return 0b01100;
        case 9:
            return 0b10000;
        default:
            return 0b01000; //  SF7
        }
    } else if(bw == 250) {
        switch(sf) {
        case 5:
            return 0b00001;
        case 6:
            return 0b00101;
        case 7:
            return 0b01001;
        case 8:
            return 0b01101;
        case 9:
            return 0b10001;
        case 10:
            return 0b10101;
        default:
            return 0b01001; //  SF7
        }
    } else if(bw == 500) {
        switch(sf) {
        case 5:
            return 0b00010;
        case 6:
            return 0b00110;
        case 7:
            return 0b01010;
        case 8:
            return 0b01110;
        case 9:
            return 0b10010;
        case 10:
            return 0b10110;
        case 11:
            return 0b11010;
        default:
            return 0b01010; //  SF7
        }
    }
    return 0b01000; //  BW125 SF7
}

static uint8_t get_subpacket_size_bits(uint16_t subpacket_size) {
    if(subpacket_size <= 32) return 0b11;
    if(subpacket_size <= 64) return 0b10;
    if(subpacket_size <= 128) return 0b01;
    return 0b00; // 200 bytes
}

static uint8_t get_transmitting_power_bits(uint8_t transmitting_power) {
    switch(transmitting_power) {
    case 22:
        return 0b00;
    case 17:
        return 0b01;
    case 13:
        return 0b10;
    case 0:
        return 0b11;
    default:
        return 0b10; //  13dBm
    }
}

static uint8_t get_wor_cycle_bits(uint16_t wor_cycle) {
    if(wor_cycle <= 500) return 0;
    if(wor_cycle >= 4000) return 7;
    return ((wor_cycle - 500) / 500);
}

bool lora_config_to_hex_string(const LoRaConfig* config, char* hex_string, size_t hex_string_size) {
    if(config == NULL || hex_string == NULL || hex_string_size < 33) {
        return false;
    }

    uint8_t uart_baud_rate_bits = get_uart_baud_rate_bits(config->baud_rate);
    uint8_t air_data_rate_bits = get_air_data_rate_bits(config->bw, config->sf);
    uint8_t subpacket_size_bits = get_subpacket_size_bits(config->subpacket_size);
    uint8_t transmitting_power_bits = get_transmitting_power_bits(config->transmitting_power);
    uint8_t wor_cycle_bits = get_wor_cycle_bits(config->wor_cycle);

    uint8_t reg0 = (uart_baud_rate_bits << 5) | air_data_rate_bits;
    uint8_t reg1 = (subpacket_size_bits << 6) | (config->rssi_ambient_noise_flag << 5) |
                   transmitting_power_bits;
    uint8_t reg2 = config->own_channel;
    uint8_t reg3 = (config->rssi_byte_flag << 7) | (config->transmission_method_type << 6) |
                   wor_cycle_bits;

    snprintf(
        hex_string,
        hex_string_size,
        "C0 00 08 %02X %02X %02X %02X %02X %02X %02X %02X",
        (uint8_t)(config->own_address >> 8),
        (uint8_t)(config->own_address & 0xFF),
        reg0,
        reg1,
        reg2,
        reg3,
        (uint8_t)(config->encryption_key >> 8),
        (uint8_t)(config->encryption_key & 0xFF));

    return true;
}
