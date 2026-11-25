#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t display_init(void);
bool display_has_panel(void);
esp_err_t display_jpeg(const uint8_t *jpeg, size_t len);
