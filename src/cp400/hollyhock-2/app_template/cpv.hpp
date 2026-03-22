#pragma once

#include <stdint.h>

namespace CPVs {

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

    typedef struct {
        char name[256];
        char path[256];
    } cpv_t;

    typedef struct {
        cpv_t *cpvs;
        int count;
    } cpvlist_t;

    const int MAX_CPVS = 256;

    void loadCPVList();
    void freeCPVList();
    cpvlist_t getCPVList();
    int loadCPVHeader(CPV_Header * header, const cpv_t * cpv);
    int loadCPVPalette(CPV_ColorPalette * palette);
    int loadCPVFrameHeader(CPV_FrameHeader * header);
    int loadCPVFrame(CPV_FrameHeader * header, uint8_t * data);
}