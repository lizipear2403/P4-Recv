#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pinout.h"

typedef struct {
    _Alignas(JPEG_DMA_ALIGNMENT) uint8_t jpeg[FRAME_MAX_SIZE];
    uint8_t seen[(FRAME_MAX_PACKETS + 7u) / 8u];
    uint16_t id;
    uint8_t total;
    uint8_t received;
    uint64_t last_fragment_us;
    bool known_id;
    bool active;
} frame_assembly_t;

typedef struct {
    uint16_t id;
    uint8_t index;
    uint8_t total;
} frame_header_t;

bool frame_read_header(const uint8_t packet[NRF_PAYLOAD_SIZE], frame_header_t *header);
void frame_start(frame_assembly_t *frame, frame_header_t header);
bool frame_add(frame_assembly_t *frame, frame_header_t header, const uint8_t *data, uint64_t now);
size_t frame_jpeg_size(const frame_assembly_t *frame);
