#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t own_address;
    uint32_t baud_rate;
    uint16_t bw;
    uint8_t sf;
    uint16_t subpacket_size;
    uint8_t rssi_ambient_noise_flag;
    uint8_t transmitting_power;
    uint8_t own_channel;
    uint8_t rssi_byte_flag;
    uint8_t transmission_method_type;
    uint16_t wor_cycle;
    uint16_t encryption_key;
} LoRaConfig;

bool lora_config_to_hex_string(const LoRaConfig* config, char* hex_string, size_t hex_string_size);

#ifdef __cplusplus
}
#endif