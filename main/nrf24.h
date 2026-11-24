#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "pinout.h"

esp_err_t nrf24_init(void);
esp_err_t nrf24_receive(uint8_t payload[NRF_PAYLOAD_SIZE], bool *received);
