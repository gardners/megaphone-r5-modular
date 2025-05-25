#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define HEIGHT 15
#define OUTPUT_HEIGHT 16
#define BASELINE 13
#define MAX_WIDTH 1024

char output[OUTPUT_HEIGHT][MAX_WIDTH + 1];

void clear_output() {
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        memset(output[y], ' ', MAX_WIDTH);
        output[y][MAX_WIDTH] = '\0';
    }
}

void draw_bitmap(FT_Bitmap *bitmap, int bitmap_top, int pen_x) {
    int y_offset = BASELINE - bitmap_top;

    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            int bx = x;
            int by = y - y_offset;

            if (by < 0 || by >= bitmap->rows)
                continue;

            int byte_index = by * bitmap->pitch + (bx >> 3);
            int bit_index = 7 - (bx & 7);
            unsigned char byte = bitmap->buffer[byte_index];
            int pixel_on = (byte >> bit_index) & 1;

            int out_x = pen_x + x;
            if (out_x >= 0 && out_x < MAX_WIDTH) {
                output[y][out_x] = pixel_on ? '#' : output[y][out_x];
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <font_file.otf> <+hex_codepoint or ascii> [...]\n", argv[0]);
        return 1;
    }

    const char* font_path = argv[1];

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not initialize FreeType\n");
        return 1;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path, 0, &face)) {
        fprintf(stderr, "Could not load font: %s\n", font_path);
        FT_Done_FreeType(ft);
        return 1;
    }

    FT_Set_Pixel_Sizes(face, 0, HEIGHT);

    clear_output();

    FT_UInt previous_glyph = 0;
    int pen_x = 0;

    for (int i = 2; i < argc; i++) {
        const char* arg = argv[i];
        size_t len = strlen(arg);
        for (size_t j = 0; j < len; j++) {
            uint32_t codepoint;
            if (j == 0 && arg[0] == '+') {
                codepoint = (uint32_t)strtol(arg + 1, NULL, 16);
                j = len;  // skip rest of string
            } else {
                codepoint = (uint8_t)arg[j];
            }

            FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
            if (glyph_index == 0) {
                fprintf(stderr, "Warning: Glyph not found for U+%04X\n", codepoint);
                continue;
            }

            // Kerning from previous glyph
            if (previous_glyph && glyph_index) {
                FT_Vector delta;
                FT_Get_Kerning(face, previous_glyph, glyph_index, FT_KERNING_DEFAULT, &delta);
                pen_x += delta.x >> 6;  // convert from 26.6 fixed to pixels
            }

            // Load and render glyph
            if (FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO | FT_LOAD_RENDER)) {
                fprintf(stderr, "Warning: Could not render U+%04X\n", codepoint);
                continue;
            }

            draw_bitmap(&face->glyph->bitmap, face->glyph->bitmap_top, pen_x);

            pen_x += face->glyph->advance.x >> 6;
            previous_glyph = glyph_index;
        }
    }

    // Trim and print the output
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        output[y][pen_x] = '\0';  // cut line at last glyph
        puts(output[y]);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
