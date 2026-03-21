#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "cpv.h"

void scale_to_8bit(int *src_w, int *src_h, int max_val) {
    if (*src_w <= max_val && *src_h <= max_val) return;

    double ratio;
    if (*src_w > *src_h) {
        ratio = (double)max_val / *src_w;
    } else {
        ratio = (double)max_val / *src_h;
    }

    *src_w = (int)(*src_w * ratio);
    *src_h = (int)(*src_h * ratio);
    
    if (*src_w == 0) *src_w = 1;
    if (*src_h == 0) *src_h = 1;
}

int get_video_dimensions(const char * filename, int * w, int * h) {
    char command[512];

    sprintf(command, 
        "ffprobe -v error -select_streams v:0 "
        "-show_entries stream=width,height "
        "-of csv=p=0 %s", filename
    );
    
    FILE * pipe = popen(command, "r");

    if (!pipe) return -1;

    if (fscanf(pipe, "%d,%d", w, h) != 2) {
        rewind(pipe); 
        if (fscanf(pipe, "%d %d", w, h) != 2) {
             fprintf(stderr, "Error: Could not read video dimensions.\n");
             pclose(pipe);
             return -1;
        }
    }

    pclose(pipe);
    return 0;
}

void build_header(CPV_Header * header, const char * filename, int max_size, int fps) {
    header->magic[0] = 'C';
    header->magic[1] = 'P';
    
    int w, h;
    get_video_dimensions(filename, &w, &h);
    scale_to_8bit(&w, &h, max_size);

    header->width = (uint8_t)w;
    header->height = (uint8_t)h;
    
    // Bit 4-7: Flags, Bit 0-3: FPS
    uint8_t flags = 0;
    header->fps_flags = (flags << 4) | (fps & 0x0F);

    header->reserved = 0;
}

void create_seed_palette(uint8_t * seed_buffer) {
    uint8_t base_hues[12][3] = {
        {255, 0, 0}, // Red
        {0, 255, 0}, // Green
        {0, 0, 255}, // Blue
        {255, 255, 0}, // Yellow
        {0, 255, 255}, // Cyan
        {255, 0, 255}, // Magenta
        {255, 128, 0}, // Orange
        {255, 0, 128}, // Pink
        {0, 255, 128}, // Spring Green/Turquoise
        {128, 0, 255}, // Violet
        {139, 69, 19}, // Brown
        {128, 128, 128} // Gray
    };

    int idx = 0;
    for (int i = 0; i < 12; i++) {
        // Dark (50% Light)
        seed_buffer[idx++] = base_hues[i][0] / 2;
        seed_buffer[idx++] = base_hues[i][1] / 2;
        seed_buffer[idx++] = base_hues[i][2] / 2;
        // Normal (100% Light)
        seed_buffer[idx++] = base_hues[i][0];
        seed_buffer[idx++] = base_hues[i][1];
        seed_buffer[idx++] = base_hues[i][2];
        // Light (Mixing with white)
        seed_buffer[idx++] = base_hues[i][0] + (255 - base_hues[i][0]) / 2;
        seed_buffer[idx++] = base_hues[i][1] + (255 - base_hues[i][1]) / 2;
        seed_buffer[idx++] = base_hues[i][2] + (255 - base_hues[i][2]) / 2;
    }
}

int run_palette_gen(CPV_Header * header, const char * filename, const char * output_filename) {
    uint8_t seed[12 * 3 * 3];
    create_seed_palette(seed);
    
    char command[1024];

    sprintf(command, 
            "ffmpeg -i \"%s\" -f rawvideo -pixel_format rgb24 -video_size 6x6 -i - "
            "-filter_complex \"[0:v]fps=%d,scale=%d:%d:flags=lanczos[vid];"
            "[1:v]scale=%d:-1:flags=neighbor[seed_scaled];"
            "[vid][seed_scaled]vstack=inputs=2[combined];"
            "[combined]palettegen=reserve_transparent=0:stats_mode=full\" "
            "-y \"%s\"", 
            filename, (header->fps_flags & 0x0F), header->width, header->height, 
            header->width, output_filename
    );
    
    FILE *pipe = popen(command, "w");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to open pipe for palette generation.\n");
        return -1;
    }
    
    fwrite(seed, 1, sizeof(seed), pipe);
    int res = pclose(pipe);
    
    if (res != 0) {
        fprintf(stderr, "Error: Palette generation failed.\n");
        return -1;
    }

    return 0;
}

int build_color_palette(CPV_ColorPalette * palette, CPV_Header * header, const char * filename) {
    const char * output_palette = "temp_palette.png";
    if (run_palette_gen(header, filename, output_palette) != 0) {
        return -1;
    }
    
    char command[1024];
    sprintf(command, "ffmpeg -i \"%s\" -f rawvideo -pix_fmt rgb24 -", output_palette);

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to open pipe for palette extraction.\n");
        return -1;
    }

    uint8_t rgb[3];
    int count = 0;
    while (fread(rgb, 1, 3, pipe) == 3 && count < 256) {
        uint16_t r = rgb[0];
        uint16_t g = rgb[1];
        uint16_t b = rgb[2];

        // Convert RGB888 to RGB565
        uint16_t r5 = (r >> 3) & 0x1F;
        uint16_t g6 = (g >> 2) & 0x3F;
        uint16_t b5 = (b >> 3) & 0x1F;
        uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5; 
        palette->colors[count] = htons(rgb565);

        count++;
    }

    pclose(pipe);
    remove(output_palette);

    return 0;
}

inline int color_dist(RGB888 color_1, RGB888 color_2) {
    int dr = (int)color_1.r - (int)color_2.r;
    int dg = (int)color_1.g - (int)color_2.g;
    int db = (int)color_1.b - (int)color_2.b;
    return dr * dr + dg * dg + db * db;
}

uint8_t find_best_index(RGB888 rgb_color, CPV_ColorPalette * palette) {
    int best_dist = 1000000;
    uint8_t best_idx = 0;

    for (int i = 0; i < 256; i++) {
        uint16_t rgb565 = ntohs(palette->colors[i]);
        RGB888 pal_rgb;
        pal_rgb.r = (uint8_t)((rgb565 >> 11) & 0x1F) << 3;
        pal_rgb.g = (uint8_t)((rgb565 >> 5) & 0x3F) << 2;
        pal_rgb.b = (uint8_t)(rgb565 & 0x1F) << 3;

        int dist = color_dist(rgb_color, pal_rgb);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = (uint8_t)i;
        }
        if (dist == 0) break;
    }
    return best_idx;
}

int build_video_data(CPV_VideoData * video_data, CPV_Header * header, CPV_ColorPalette * palette, const char * filename) {
    char command[1024];

    sprintf(command, "ffmpeg -i \"%s\" -vf \"fps=%d,scale=%d:%d:flags=lanczos,hqdn3d\" -f rawvideo -pix_fmt rgb24 -", 
            filename, (header->fps_flags & 0x0F), header->width, header->height
    );
        
    FILE * pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to open pipe for video conversion.\n");
        return -1;
    }

    int frame_size_pixels = header->width * header->height;
    uint8_t * rgb_buffer = malloc(frame_size_pixels * 3);
    uint8_t * current_index_buffer = malloc(frame_size_pixels);
    uint8_t * previous_index_buffer = calloc(frame_size_pixels, 1);

    video_data->size = 0;
    video_data->data = NULL;

    int frame_count = 0;
    while (fread(rgb_buffer, 1, frame_size_pixels * 3, pipe) == (size_t)(frame_size_pixels * 3)) {
        // convert rgb to index
        for (int i = 0; i < frame_size_pixels; i++) {
            RGB888 current_pixel;
            current_pixel.r = rgb_buffer[i * 3 + 0];
            current_pixel.g = rgb_buffer[i * 3 + 1];
            current_pixel.b = rgb_buffer[i * 3 + 2];
            current_index_buffer[i] = find_best_index(current_pixel, palette);
        }
        
        // apply delta compression & run-length encoding
        uint8_t * frame_buffer = malloc(frame_size_pixels * 4);
        int f_ptr = 2;

        for (int i = 0; i < frame_size_pixels; ) {
            int skip_len = 0;
            while (i + skip_len < frame_size_pixels && current_index_buffer[i + skip_len] == previous_index_buffer[i + skip_len]) {
                skip_len++;
            }

            if (skip_len > 0) {
                int temp_skip = skip_len;
                while (temp_skip > 0) {
                    if (temp_skip <= 63) {
                        frame_buffer[f_ptr++] = 0x80 | ((uint8_t)temp_skip & 0x3F); // Short Skip 10xx
                        temp_skip = 0;
                    } else {
                        uint16_t chunk = (temp_skip > 16383) ? 16383 : (uint16_t)temp_skip;
                        uint16_t packet = htons(0xC000 | (chunk & 0x3FFF)); // Long Skip 11xxxxxx
                        memcpy(&frame_buffer[f_ptr], &packet, 2);
                        f_ptr += 2;
                        temp_skip -= chunk;
                    }
                }
                i += skip_len;
            }

            if (i >= frame_size_pixels) break;

            uint8_t target_color = current_index_buffer[i];
            int data_len = 0;

            while (i + data_len < frame_size_pixels && 
                   current_index_buffer[i + data_len] == target_color && 
                   current_index_buffer[i + data_len] != previous_index_buffer[i + data_len]) {
                data_len++;
            }

            if (data_len > 0) {
                int temp_data = data_len;
                while (temp_data > 0) {
                    if (temp_data <= 63) {
                        frame_buffer[f_ptr++] = 0x00 | ((uint8_t)temp_data & 0x3F); // Short Data 00xx
                        frame_buffer[f_ptr++] = target_color;
                        temp_data = 0;
                    } else {
                        uint16_t chunk = (temp_data > 16383) ? 16383 : (uint16_t)temp_data;
                        uint16_t packet = htons(0x4000 | (chunk & 0x3FFF)); // Long Data 01xxxxxx
                        memcpy(&frame_buffer[f_ptr], &packet, 2);
                        f_ptr += 2;
                        frame_buffer[f_ptr++] = target_color;
                        temp_data -= chunk;
                    }
                }
                i += data_len;
            }
        }

        uint16_t actual_size = htons((uint16_t)(f_ptr - 2));
        memcpy(frame_buffer, &actual_size, 2);

        video_data->data = realloc(video_data->data, video_data->size + f_ptr);
        memcpy(video_data->data + video_data->size, frame_buffer, f_ptr);
        video_data->size += f_ptr;

        memcpy(previous_index_buffer, current_index_buffer, frame_size_pixels);
        free(frame_buffer);

        if (frame_count % 100 == 0) printf("Frame %d encodet...\n", frame_count);
        frame_count++;
    }

    free(rgb_buffer);
    free(current_index_buffer);
    free(previous_index_buffer);
    pclose(pipe);
    return 0;
}

int main(int argc, char *argv[]) {
    char * filename = NULL;
    int max_size = 80;
    int fps = 5;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-max_size") == 0 && i + 1 < argc) {
            max_size = atoi(argv[++i]);
            if (max_size < 1) max_size = 1;
            if (max_size > 255) max_size = 255;
        } else if (strcmp(argv[i], "-fps") == 0 && i + 1 < argc) {
            fps = atoi(argv[++i]);
            if (fps < 1) fps = 1;
            if (fps > 15) fps = 15;
        } else {
            filename = argv[i];
        }
    }

    if (!filename || !(2 <= argc <= 6)) {
        printf("Usage: %s <videofile> [-max_size <1-255>] [-fps <1-15>]\n", argv[0]);
        printf("Out: <videofile>.cpv\n");
        return -1;
    }
    
    // collect meta data for my own format
    CPV_Header * header = malloc(sizeof(CPV_Header));
    if (header == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for header\n");
        return -1;
    }
    build_header(header, filename, max_size, fps);
    
    // get an optimal color palette
    CPV_ColorPalette * palette = malloc(sizeof(CPV_ColorPalette));
    if (palette == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for palette\n");
        free(header);
        return -1;
    }

    if (build_color_palette(palette, header, filename) != 0) {
        return -1;
    }

    // convert the video data frame per frame
    CPV_VideoData * video_data = malloc(sizeof(CPV_VideoData));
    if (video_data == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for video data\n");
        free(palette);
        free(header);
        return -1;
    }

    build_video_data(video_data, header, palette, filename);

    // build the video file
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "%s.cpv", filename);

    FILE *out_file = fopen(output_filename, "wb");
    if (!out_file) {
        fprintf(stderr, "Error: Could not open output file %s\n", output_filename);
        free(video_data->data);
        free(video_data);
        free(palette);
        free(header);
        return -1;
    }

    fwrite(header, sizeof(CPV_Header), 1, out_file);
    fwrite(palette, sizeof(CPV_ColorPalette), 1, out_file);
    fwrite(video_data->data, 1, video_data->size, out_file);

    // cleanup
    fclose(out_file);
    printf("Successfully created %s (%d bytes)\n", output_filename, video_data->size);
    free(video_data->data);
    free(video_data);
    free(palette);
    free(header);
    return 0;
}
