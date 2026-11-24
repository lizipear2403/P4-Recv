/* RP2040 的 FramePkt 头为小端 id:16 / index:8 / total:8，尾包以零补齐。 */
#include "frame.h"
#include "Mask.h"

#include <string.h>

bool frame_read_header(const uint8_t packet[NRF_PAYLOAD_SIZE], frame_header_t *header)
{
    const uint32_t info = (uint32_t)packet[0] | ((uint32_t)packet[1] << 8)
        | ((uint32_t)packet[2] << 16) | ((uint32_t)packet[3] << 24);
    header->id = (info & FRAME_ID_MASK) >> FRAME_ID_OFFSET;
    header->index = (info & SUB_FRAME_MASK) >> SUB_FRAME_OFFSET;
    header->total = (info & EXPECT_PKT_NUM) >> EXPECT_PKT_OFFSET;
    return header->index < header->total;
}

void frame_start(frame_assembly_t *frame, frame_header_t header)
{
    memset(frame->seen, 0, sizeof(frame->seen));
    frame->id = header.id;
    frame->total = header.total;
    frame->received = 0;
    frame->known_id = true;
    frame->active = true;
}

bool frame_add(frame_assembly_t *frame, frame_header_t header, const uint8_t *data, uint64_t now)
{
    uint8_t *bitmap_byte = &frame->seen[header.index / 8u];
    const uint8_t bit = 1u << (header.index % 8u);
    if (*bitmap_byte & bit) {
        return false;
    }
    memcpy(frame->jpeg + header.index * FRAME_FRAGMENT_SIZE, data, FRAME_FRAGMENT_SIZE);
    *bitmap_byte |= bit;
    frame->last_fragment_us = now;
    return ++frame->received == frame->total;
}

size_t frame_jpeg_size(const frame_assembly_t *frame)
{
    if (!frame->total || frame->received != frame->total) {
        return 0;
    }
    size_t len = frame->total * FRAME_FRAGMENT_SIZE;
    const size_t last_offset = len - FRAME_FRAGMENT_SIZE;
    while (len > last_offset && frame->jpeg[len - 1] == 0) {
        --len;
    }
    if (len <= last_offset || len < 4 ||
        frame->jpeg[0] != 0xff || frame->jpeg[1] != 0xd8 ||
        frame->jpeg[len - 2] != 0xff || frame->jpeg[len - 1] != 0xd9) {
        return 0;
    }
    return len;
}
