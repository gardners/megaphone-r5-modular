#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define WIDTH 16
#define HEIGHT 15

void print_ascii_art(FT_Bitmap *bitmap) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (x >= bitmap->width || y >= bitmap->rows) {
                putchar(' ');
            } else {
                int byte_index = y * bitmap->pitch + (x >> 3);  // byte in row
                int bit_index = 7 - (x & 7);                    // MSB first
                unsigned char byte = bitmap->buffer[byte_index];
                int pixel_on = (byte >> bit_index) & 1;
                putchar(pixel_on ? '#' : ' ');
            }
        }
        putchar('\n');
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <font_file.otf> <unicode_hex>\n", argv[0]);
        return 1;
    }

    const char* font_path = argv[1];
    uint32_t codepoint = (uint32_t)strtol(argv[2], NULL, 16);

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

    printf("Font units_per_EM: %ld\n", face->units_per_EM);
    printf("Ascender: %ld\n", face->ascender);
    printf("Descender: %d\n", (int)face->descender);
    printf("Height: %ld\n", face->height);

    
    FT_Set_Pixel_Sizes(face, 0, HEIGHT);

    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);

    if (glyph_index == 0) {
        fprintf(stderr, "Glyph not found for codepoint U+%04X\n", codepoint);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return 1;
    }

    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO | FT_LOAD_RENDER)) {
        fprintf(stderr, "Could not load/render glyph for U+%04X\n", codepoint);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return 1;
    }

    FT_Bitmap *bitmap = &face->glyph->bitmap;
    print_ascii_art(bitmap);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}

