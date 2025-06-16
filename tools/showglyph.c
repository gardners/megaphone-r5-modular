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

FILE *outfile=NULL;

void record_glyph(uint32_t codepoint, unsigned char data[256])
{
  if (outfile) {
    fseek(outfile,codepoint << 8, SEEK_SET);
    fwrite(data,256,1,outfile);
  }
}


int dump_bytes(char *msg, unsigned char *bytes, int length)
{
  fprintf(stdout, "%s:\n", msg);
  for (int i = 0; i < length; i += 16) {
    fprintf(stdout, "%04X: ", i);
    for (int j = 0; j < 16; j++)
      if (i + j < length)
        fprintf(stdout, " %02X", bytes[i + j]);
    fprintf(stdout, "\n");
  }
  return 0;
}

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

int calc_colour_error(int is_color, int is_wide, uint8_t original, uint8_t substitute) {
    if (is_color) {
        // 332 RGB: RRR GGG BB
        int ro = (original >> 5) & 0x07;
        int go = (original >> 2) & 0x07;
        int bo = (original >> 0) & 0x03;

        int rs = (substitute >> 5) & 0x07;
        int gs = (substitute >> 2) & 0x07;
        int bs = (substitute >> 0) & 0x03;

        int dr = ro - rs;
        int dg = go - gs;
        int db = bo - bs;

        return 3 * dr * dr + 6 * dg * dg + 1 * db * db;
    } else if (is_wide) {
        // 4-bit intensity packed into a byte: high and low nybbles
        int lo_orig = (original >> 4) & 0x0F;
        int hi_orig = original & 0x0F;

        int lo_subs = (substitute >> 4) & 0x0F;
        int hi_subs = substitute & 0x0F;

        return abs(hi_orig - hi_subs) + abs(lo_orig - lo_subs);
    } else {
        // 8-bit grayscale intensity
        int diff = (int)original - (int)substitute;
        return diff * diff;
    }
}

/*  Copy one glyph from pixel_data[][16] → data[256]
 *
 *  ┌───────────────  glyph columns ───────────────┐
 *  row 0  pixel_data[0][ 0.. 7] → data[0.. 63]  (tile 0, row 0)
 *         pixel_data[0][ 8..15] → data[128..191] (tile 2, row 0)
 *  row 1  pixel_data[1][ 0.. 7] → data[64..127] (tile 1, row 0)
 *         pixel_data[1][ 8..15] → data[192..255] (tile 3, row 0)
 *  row 2  pixel_data[2][ 0.. 7] → data[0.. 63]  (tile 0, row 1)
 *  ...
 *
 *  Layout recap
 *  ┌─────┬─────┐         data[  0.. 63]  = tile 0 (even rows, left 8 bytes)
 *  │ T0  │ T2  │         data[128..191]  = tile 2 (even rows, right 8 bytes)
 *  ├─────┼─────┤
 *  │ T1  │ T3  │         data[ 64..127]  = tile 1 (odd rows,  left 8 bytes)
 *  └─────┴─────┘         data[192..255]  = tile 3 (odd rows,  right 8 bytes)
 */
static void
pack_into_tiles(const uint8_t pixel_data[OUTPUT_HEIGHT][16], uint8_t data[256])
{
    memset(data, 0, 256);          /*  safety / predictable padding           */

    for (int y = 0; y < OUTPUT_HEIGHT; ++y) {          /* 0‥15 */
        const int tile_row     = y >> 1;               /* 0‥7   */
        const int even_row     = !(y & 1);             /* true if 0,2,4…       */
        const int even_offset  = even_row ?   0 : 64; /* tiles 0+1 or 2+3     */

        for (int bx = 0; bx < 16; ++bx) {              /* byte-column 0‥15     */
            const int right_half  = (bx >= 8);         /* 0 = left, 1 = right  */
            const int base_offset = even_offset + (right_half ? 128 : 0);
            const int dest_index  = base_offset + tile_row * 8 + (bx & 7);

            data[dest_index] = pixel_data[y][bx];
        }
    }
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

    if (width > 31 ) {
      fprintf(stderr,"WARNING: U+%05x is %dpx wide. Will be truncated to 32px\n",codepoint, width);
      width = 32;
    }
    
    uint8_t flag_byte = 0;
    if (is_color) flag_byte |= 0x01;
    if (is_wide) flag_byte |= 0x02;

    uint8_t glyph_width = width;  // For kerning-aware spacing

    // Allocate pixel buffer
    uint8_t pixel_data[OUTPUT_HEIGHT][16] = {{0}}; // max 16px wide (or 32px with nybl packing, so 16 bytes, either way)

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
                    if (x % 2 == 1) {
		      pixel_data[y][x / 2] &= 0x0f;
		      pixel_data[y][x / 2] |= (value >> 4) << 4;
                    } else {
		      pixel_data[y][x / 2] &= 0xf0;
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
		  if (x % 2 == 1) {
		    pixel_data[y][x / 2] &= 0x0f;
		    pixel_data[y][x / 2] |= (value >> 4) << 4;
		  } else {
		    pixel_data[y][x / 2] &= 0xf0;
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
    unsigned char data[256];

    pack_into_tiles(pixel_data, data);
    
    // Overwrite last byte with flags
    // Width is in range 0..32, so just put it in 5 bits.
    // is_wide can be implied from whether it's >15px wide, so doesn't need
    // to be recorded explicitly.
    int pixel_select = -1;
    int colour_error = 0xfffffff;

    int this_colour_error = calc_colour_error(is_color, is_wide, data[255],data[254]);
    if (this_colour_error < colour_error) { pixel_select = 0; colour_error = 0; }
    this_colour_error = calc_colour_error(is_color, is_wide, data[255],data[127]);
    if (this_colour_error < colour_error) { pixel_select = 0; colour_error = 1; }
    this_colour_error = calc_colour_error(is_color, is_wide, data[255],0);
    if (this_colour_error < colour_error) { pixel_select = 0; colour_error = 2; }
    this_colour_error = calc_colour_error(is_color, is_wide, data[255],255);
    if (this_colour_error < colour_error) { pixel_select = 0; colour_error = 3; }
        
    data[255]
      =( is_color ? 0x80 : 0x00 )
      | (width>0 ? width - 1 : 0)
      | ( pixel_select << 5);
      
    dump_bytes("Packed glyph",data,256);

    record_glyph(codepoint, data);
    
}


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <font_file.otf> [+hex_codepoint or ascii] [...]\n", argv[0]);
        return 1;
    }

    const char* font_path = argv[1];

    char filename[8192];
    snprintf(filename,8192,"%s.MRF",argv[1]);
    outfile=fopen(filename,"wb");
    
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
