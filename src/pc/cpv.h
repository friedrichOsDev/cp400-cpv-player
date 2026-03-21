#pragma once

#include <stdint.h>

typedef struct {
    uint8_t magic[2]; // "CP" / 0x43 0x50
    uint8_t width;
    uint8_t height;
    uint8_t fps_flags; // Bit 4-7: Flags, Bit 0-3: FPS
    uint8_t reserved;
} __attribute__((packed)) CPV_Header;

typedef uint16_t RGB565; // RGB565
typedef struct {
    uint8_t r, g, b;
} __attribute__((packed)) RGB888; // RGB888
typedef RGB565 CPV_Color;

typedef struct {
    CPV_Color colors[256];
} __attribute__((packed)) CPV_ColorPalette;

typedef struct {
    uint16_t size;
} __attribute__((packed)) CPV_FrameHeader;

typedef struct {
    int size;
    uint8_t * data;
} CPV_VideoData;

/* unused structs - only for understanding my RLE format
typedef struct {
    uint8_t length_flags; // Bit 7: 1, Bit 6: 0, Bit 0-5: length (max 63)
} __attribute__((packed)) CPV_RLE_DataSkipShort;

typedef struct {
    uint16_t length_flags; // Bit 15: 1, Bit 14: 1, Bit 0-13: length (max 16383)
} __attribute__((packed)) CPV_RLE_DataSkipLong;

typedef struct {
    uint8_t length_flags; // Bit 7: 0, Bit 6: 0, Bit 0-5: length (max 63)
    uint8_t color_index;
} __attribute__((packed)) CPV_RLE_DataShort;

typedef struct {
    uint16_t length_flags; // Bit 15: 0, Bit 14: 1, Bit 0-13: length (max 16383)
    uint8_t color_index;
} __attribute__((packed)) CPV_RLE_DataLong;

typedef struct{
    uint16_t size;
    union {
        CPV_RLE_DataSkipShort skip_short;
        CPV_RLE_DataSkipLong skip_long;
        CPV_RLE_DataShort data_short;
        CPV_RLE_DataLong data_long;
    } * rle_data;
} CPV_FrameData;
*/