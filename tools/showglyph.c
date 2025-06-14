#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define HEIGHT 17
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

char intensity_to_ascii(uint8_t intensity) {
    // Density ramp from light to dark
    const char ramp[] = " .:-=+*#%@";
    const int ramp_len = sizeof(ramp) - 1; // exclude null terminator

    int index = (intensity * ramp_len) / 256;
    if (index >= ramp_len) index = ramp_len - 1;

    return ramp[index];
}

void render_glyph(FT_Face face, uint32_t codepoint, int pen_x) {
    clear_output();

    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index == 0) {
        fprintf(stderr, "Warning: Glyph not found for U+%04X\n", codepoint);
        return;
    }

#if 0
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO)) {
        fprintf(stderr, "Warning: Could not render U+%04X in mono\n", codepoint);
    } else  {
      draw_bitmap(&face->glyph->bitmap, face->glyph->bitmap_top, pen_x);
    }
#endif
    
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
        fprintf(stderr, "Warning: Could not render U+%04X\n", codepoint);
        return;
    }

    
    FT_Bitmap *bmp = &face->glyph->bitmap;
    int width = face->glyph->advance.x >> 6;
    int is_color = (bmp->pixel_mode == FT_PIXEL_MODE_BGRA);
    int is_wide = (width > 15);

    uint8_t flag_byte = 0;
    if (is_color) flag_byte |= 0x01;
    if (is_wide) flag_byte |= 0x02;

    uint8_t glyph_width = width;  // For kerning-aware spacing

    // Allocate pixel buffer
    uint8_t pixel_data[OUTPUT_HEIGHT][32] = {{0}}; // max 32px wide

    int y_offset = BASELINE - face->glyph->bitmap_top;

    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        for (int x = 0; x < bmp->width && x < (is_wide ? 32 : 16); x++) {
            int by = y - y_offset;
            if (by < 0 || by >= bmp->rows) continue;

            uint8_t value = 0;

            if (is_color && bmp->pixel_mode == FT_PIXEL_MODE_BGRA) {
                uint8_t *p = &bmp->buffer[(by * bmp->width + x) * 4];
                uint8_t a = p[3];
                if (a < 16) continue;  // transparent
                // 332 color encoding
                uint8_t r = p[2] >> 5;
                uint8_t g = p[1] >> 5;
                uint8_t b = p[0] >> 6;
                value = (r << 5) | (g << 2) | b;
                if (value == 0) value = 0x01;  // 0 reserved for transparent
            }
            else if (bmp->pixel_mode == FT_PIXEL_MODE_GRAY) {
                value = bmp->buffer[by * bmp->pitch + x];
                if (!is_wide) {
                    pixel_data[y][x] = value;
                } else {
                    // 4-bit: pack 2 pixels per byte
                    if (x % 2 == 0) {
                        pixel_data[y][x / 2] = (value >> 4) << 4;
                    } else {
                        pixel_data[y][x / 2] |= (value >> 4);
                    }
                }
            }
            else if (bmp->pixel_mode == FT_PIXEL_MODE_MONO) {
                int byte = bmp->buffer[by * bmp->pitch + (x >> 3)];
                int bit = (byte >> (7 - (x & 7))) & 1;
                value = bit ? 255 : 0;
                if (!is_wide) {
                    pixel_data[y][x] = value;
                } else {
                    if (x % 2 == 0) {
                        pixel_data[y][x / 2] = (value >> 4) << 4;
                    } else {
                        pixel_data[y][x / 2] |= (value >> 4);
                    }
                }
            }
        }
    }

    // Optional: Print debug info
    printf("U+%04X (%s, %dpx): flags=0x%02X, width=%d\n", codepoint,
           is_color ? "color" : "intensity",
           width, flag_byte, glyph_width);

    pen_x = 0;
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
      for(int x=0;x<glyph_width;x++) {
	output[y][pen_x + x] = intensity_to_ascii(pixel_data[y][x]);
      }
      output[y][pen_x + glyph_width] = '|';
      output[y][pen_x + glyph_width + 1] = 0;
      puts(output[y]);
    }
    puts("");

    // TODO: Save `pixel_data`, `flag_byte`, `glyph_width` somewhere
}


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <font_file.otf> [+hex_codepoint or ascii] [...]\n", argv[0]);
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

    int pen_x = 0;

    if (argc==2) {
      FT_ULong charcode;
      FT_UInt glyph_index;
      int pen_x = 0;
      
      charcode = FT_Get_First_Char(face, &glyph_index);
      while (glyph_index != 0) {
	render_glyph(face, charcode, pen_x);
	// Optionally: pen_x += face->glyph->advance.x >> 6; (not used in per-glyph rendering)
	charcode = FT_Get_Next_Char(face, charcode, &glyph_index);
      }      
    } else {
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
	  
	  
	  render_glyph(face, codepoint,pen_x);
	  
	  // pen_x += face->glyph->advance.x >> 6;
        }
      }
      
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
