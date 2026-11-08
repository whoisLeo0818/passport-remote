#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Little-endian PWF1: 64-byte header, 1024-byte JSON area, RGB565 sections.
 * The browser performs resizing; firmware never allocates a whole image. */
enum { TRANSFER_HEADER=64, TRANSFER_JSON=1024, TRANSFER_PROFILE=1, TRANSFER_PHOTO=2 };
typedef struct {
    uint32_t kind, length, crc;
    uint16_t width[6], height[6];
} passport_transfer_t;
bool passport_transfer_parse(const uint8_t header[64], size_t total, passport_transfer_t *out);
uint32_t passport_crc32(uint32_t previous, const void *data, size_t length);
