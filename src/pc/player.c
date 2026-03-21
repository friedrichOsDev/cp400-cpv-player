#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include "cpv.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file.cpv>\n", argv[0]);
        return -1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return -1;
    }

    CPV_Header header;
    if (fread(&header, sizeof(CPV_Header), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read header.\n");
        fclose(file);
        return -1;
    }
    
    if (header.magic[0] != 'C' || header.magic[1] != 'P') {
        fprintf(stderr, "Error: Invalid magic number. Not a CPV file.\n");
        fclose(file);
        return -1;
    }

    CPV_ColorPalette palette;
    if (fread(&palette, sizeof(CPV_ColorPalette), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read color palette.\n");
        fclose(file);
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        fclose(file);
        return -1;
    }

    int scale = 1;
    if (header.width > 0) {
        scale = 640 / header.width;
        if (scale < 1) scale = 1;
        if (scale > 10) scale = 10;
    }

    SDL_Window *window = SDL_CreateWindow("CPV Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, header.width * scale, header.height * scale, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, header.width, header.height);

    uint32_t total_pixels = header.width * header.height;
    uint8_t *index_buffer = calloc(total_pixels, 1);
    uint32_t *pixel_buffer = malloc(total_pixels * sizeof(uint32_t));

    int fps = header.fps_flags & 0x0F;
    int frame_delay = 1000 / (fps > 0 ? fps : 10);
    int quit = 0;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
        }

        uint16_t frame_size_be;
        if (fread(&frame_size_be, 2, 1, file) != 1) {
            rewind(file);
            fseek(file, sizeof(CPV_Header) + sizeof(CPV_ColorPalette), SEEK_SET);
            memset(index_buffer, 0, total_pixels);
            continue;
        }
        uint16_t frame_size = ntohs(frame_size_be);

        uint8_t *compressed_data = malloc(frame_size);
        if (!compressed_data) break;

        if (fread(compressed_data, 1, frame_size, file) != frame_size) {
            fprintf(stderr, "Error: Failed to read compressed frame data.\n");
            free(compressed_data);
            break;
        }

        uint32_t p_idx = 0; 
        uint32_t c_ptr = 0; 
        
        while (c_ptr < frame_size && p_idx < total_pixels) {
            uint8_t first_byte = compressed_data[c_ptr];
            
            if (first_byte & 0x80) { // SKIP
                uint16_t length;
                if (first_byte & 0x40) { // Long Skip (11)
                    length = ntohs(*(uint16_t*)(compressed_data + c_ptr)) & 0x3FFF;
                    c_ptr += 2;
                } else { // Short Skip (10)
                    length = first_byte & 0x3F;
                    c_ptr += 1;
                }
                p_idx += length;
            } else { // DATA
                uint16_t length;
                uint8_t color_idx;
                if (first_byte & 0x40) { // Long Data (01)
                    length = ntohs(*(uint16_t*)(compressed_data + c_ptr)) & 0x3FFF;
                    color_idx = compressed_data[c_ptr + 2];
                    c_ptr += 3;
                } else { // Short Data (00)
                    length = first_byte & 0x3F;
                    color_idx = compressed_data[c_ptr + 1];
                    c_ptr += 2;
                }
                for (uint16_t i = 0; i < length && p_idx < total_pixels; i++) {
                    index_buffer[p_idx++] = color_idx;
                }
            }
        }

        for (uint32_t i = 0; i < total_pixels; i++) {
            uint16_t color = ntohs(palette.colors[index_buffer[i]]);
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t b = (color & 0x1F) << 3;
            pixel_buffer[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }

        SDL_UpdateTexture(texture, NULL, pixel_buffer, header.width * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        free(compressed_data);
        SDL_Delay(frame_delay);
    }

    free(index_buffer);
    free(pixel_buffer);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    fclose(file);
    return 0;
}