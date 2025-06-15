#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

#define NUM_FONTS 4
#define FONT_EMOJI_COLOUR 0
#define FONT_EMOJI_MONO 1
#define FONT_TEXT 2
#define FONT_UI 3
char *font_files[NUM_FONTS]={"NotoColorEmoji","NotoEmoji", "NotoSans", "Nokia Pixel Large"};
struct shared_resource fonts[NUM_FONTS];
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

unsigned char buffer[128];

unsigned char i;

unsigned long screen_ram = 0x12000;
unsigned long colour_ram = 0xff80800L;

void screen_setup(void)
{
  // Blue background, black border
  POKE(0xD020,0);
  POKE(0xD021,6);

  // H640 + fast CPU
  POKE(0xD031,0xc0);  
  
  // 16-bit text mode, alpha compositor, 40MHz
  POKE(0xD054,0xC5);

  // PAL
  POKE(0xD06f,0x00);
  
  // Retract borders to be 1px
  POKE(0xD05C,0x3B);
  POKE(0xD048,0x41); POKE(0xD049,0x00);
  POKE(0xD04A,0x20); POKE(0xD04B,0x02);  

  // 90 columns wide (but with virtual line length of 255)
  // Advance 512 bytes per line
  POKE(0xD058,0x00); POKE(0xD059,0x02);
  // XXX -- We can display more than 128, but then we need to insert GOTOX tokens to prevent RRB wrap-around
  POKE(0xD05E,0x80); // display 128 
  
  // 30 rows
  POKE(0xD07B,30 - 1);
  
  // Chargen vertically centred for 30 rows, and at left-edge of 720px display
  // (We _could_ use all 800px horizontally on the phone display, but then we can't see it on VGA output for development/debugging)
  POKE(0xD04C,0x3B); POKE(0xD04D,0x00);
  POKE(0xD04E,0x41); POKE(0xD04F,0x00);
  
  // Double-height char mode
  POKE(0xD07A,0x10);

  // Colour RAM offset
  POKE(0xD064,colour_ram>>0);
  POKE(0xD065,colour_ram>>8);
  
  // Screen RAM address
  POKE(0xD060,((unsigned long)screen_ram)>>0);
  POKE(0xD061,((unsigned long)screen_ram)>>8);
  POKE(0xD062,((unsigned long)screen_ram)>>16);

}

void screen_clear(void)
{
  // Clear screen RAM
  lfill(screen_ram,0x00,(90*30*2));

  // Clear colour RAM
  lfill(colour_ram,0x01,(90*30*2));
  
}


unsigned char nybl_swap(unsigned char v) {
    return ((v & 0x0F) << 4) | ((v & 0xF0) >> 4);
}

void generate_rgb332_palette(void)
{
  unsigned int i;
  
  // Select Palette 1 for access at $D100-$D3FF
  POKE(0xD070,0x40);
  
  for (i = 0; i < 256; i++) {
    // RGB332 bit layout:
    // Bits 7-5: Red (3 bits)
    // Bits 4-2: Green (3 bits)
    // Bits 1-0: Blue (2 bits)
    
    // Extract components and scale to 8-bit
    unsigned char red   = ((i >> 5) & 0x07) * 255 / 7;
    unsigned char green = ((i >> 2) & 0x07) * 255 / 7;
    unsigned char blue  = ( i       & 0x03) * 255 / 3;
    
    // Nybl-swap each value
    unsigned char red_swapped   = nybl_swap(red);
    unsigned char green_swapped = nybl_swap(green);
    unsigned char blue_swapped  = nybl_swap(blue);
    
    // Write to MEGA65 palette memory
    POKE(0xD100L + i, red_swapped);
    POKE(0xD200L + i, green_swapped);
    POKE(0xD300L + i, blue_swapped);
  }

  // Select Palette 0 for access at $D100-$D3FF,
  // use Palette 1 (set above) for alternate colour palette
  // (this is what we will use for unicode colour glyphs)
  POKE(0xD070,0x01);

}


// 128KB buffer for 128KB / 256 bytes per glyph = 512 unique unicode glyphs on screen at once
#define GLYPH_DATA_START 0x40000
#define GLYPH_CACHE_SIZE 512
#define BYTES_PER_GLYPH 256
unsigned long cached_codepoints[GLYPH_CACHE_SIZE];
unsigned char cached_fontnums[GLYPH_CACHE_SIZE];
unsigned char cached_glyph_flags[GLYPH_CACHE_SIZE];
unsigned char glyph_buffer[BYTES_PER_GLYPH];

void reset_glyph_cache(void)
{
  lfill(GLYPH_DATA_START,0x00,GLYPH_CACHE_SIZE * BYTES_PER_GLYPH);
  lfill((unsigned long)cached_codepoints,0x00,GLYPH_CACHE_SIZE*sizeof(unsigned long));
}

void load_glyph(int font, unsigned long codepoint, unsigned int cache_slot)
{
  unsigned char glyph_flags;
  shseek(&fonts[font],codepoint<<8,SEEK_SET);
  for(glyph_flags=0;glyph_flags<255;glyph_flags++) glyph_buffer[glyph_flags]=glyph_flags;
  glyph_buffer[0xff]=0xf;
  //  shread(glyph_buffer,256,&fonts[font]);

  // Extract glyph flags
  glyph_flags = glyph_buffer[0xff];

  // Replace glyph flag byte with indicated pixel value
  switch((glyph_flags>>5)&0x03) {
  case 0: glyph_buffer[0xff]=glyph_buffer[0xfe];
  case 1: glyph_buffer[0xff]=glyph_buffer[0x7f];
  case 2: glyph_buffer[0xff]=0;
  case 3: glyph_buffer[0xff]=0xff;
  }

  // Store glyph in the cache
  lcopy((unsigned long)glyph_buffer,GLYPH_DATA_START + ((unsigned long)cache_slot<<8), BYTES_PER_GLYPH);
  cached_codepoints[cache_slot]=codepoint;
  cached_fontnums[cache_slot]=font;
  cached_glyph_flags[cache_slot]=glyph_flags;
}

extern unsigned char screen_ram_1_left[64];
extern unsigned char screen_ram_1_right[64];
extern unsigned char colour_ram_0_left[64];
extern unsigned char colour_ram_0_right[64];
extern unsigned char colour_ram_1[64];

unsigned int row0_offset,row1_offset;

char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour)
{
  unsigned int i;
  unsigned char table_index; 

  colour &= 0x0f;
  
  for(i=0;i<GLYPH_CACHE_SIZE;i++) {
    if (!cached_codepoints[i]) break;
    if (cached_codepoints[i]==codepoint&&cached_fontnums[i]==font) break;
  }

  if (i==GLYPH_CACHE_SIZE) {
    // Cache full! We cannot draw this glyph.
    // XXX - Consider cache purging mechanisms, e.g., checking if all
    // glyphs in the cache are currently still on screen?
    // Should we reserve an entry in the cache slot for "unprintable glyph"?
    // (maybe just show it as [+HEX] for now? But that would be up to the calling
    // function to decide).
    return 0;
  }

  if (cached_codepoints[i]!=codepoint) {
    load_glyph(font, codepoint, i);
  } 

  /*
    Draw the glyph in the indicated position in the screen RAM.
    Note that it is not the actual pixel coordinates on the screen.
    (Recall that we are using GOTOX after drawing the base layer of the screen, to then
     draw the variable-width parts of the interface over the top.)

    What we do do here, though, is set the glyph width bits in the screen RAM.
    We want this routine to be as fast as possible, so we maintain a cache of the colour RAM
    byte values based on every possible glyph_flags byte value.
    
  */
  
  // XXX -- Actually set the character pointers here

  // Construct 6-bit table index entry
  table_index = cached_glyph_flags[i]&0x1f;
  table_index |= (cached_glyph_flags[i]>>2)&0x20;

  // Get offset within screen and colour RAM for both rows of chars
  row0_offset = (y<<9) + (x<<1);
  row1_offset = row0_offset + 512;

  // Set screen RAM
  lpoke(screen_ram + row0_offset + 0, ((i&0x3f)<<2) + 0 );
  lpoke(screen_ram + row1_offset + 0, ((i&0x3f)<<2) + 2 );
  lpoke(screen_ram + row0_offset + 1, screen_ram_1_left[table_index] + (i>>6) + 0x10);
  lpoke(screen_ram + row1_offset + 1, screen_ram_1_left[table_index] + (i>>6) + 0x10);

  // Set colour RAM
  lpoke(colour_ram + row0_offset + 0, colour_ram_0_left[table_index]);
  lpoke(colour_ram + row1_offset + 0, colour_ram_0_left[table_index]);
  lpoke(colour_ram + row0_offset + 1, colour_ram_1[table_index]+colour);
  lpoke(colour_ram + row1_offset + 1, colour_ram_1[table_index]+colour);

  // And the 2nd column, if required
  if (cached_glyph_flags[i]&0x18) {
    // Screen RAM
    lpoke(screen_ram + row0_offset + 2, ((i&0x3f)<<2) + 1 );
    lpoke(screen_ram + row0_offset + 3, screen_ram_1_right[table_index] + (i>>6) + 0x10);
    lpoke(screen_ram + row1_offset + 2, ((i&0x3f)<<2) + 3);
    lpoke(screen_ram + row1_offset + 3, screen_ram_1_right[table_index] + (i>>6) + 0x10);

    // Colour Ram
    lpoke(colour_ram + row0_offset + 2, colour_ram_0_right[table_index]);
    lpoke(colour_ram + row1_offset + 2, colour_ram_0_right[table_index]);
    lpoke(colour_ram + row0_offset + 3, colour_ram_1[table_index]+colour);
    lpoke(colour_ram + row1_offset + 3, colour_ram_1[table_index]+colour);

    // Rendered as 2 chars wide
    // XXX also report number of pixels consumed
    return 2;
  }

  // Rendered as 1 char wide
  // XXX also report number of pixels consumed
  return 1;
}

void main(void)
{
  shared_resource_dir d;

  mega65_io_enable();

  screen_setup();
  screen_clear();

  generate_rgb332_palette();
  
  // Make sure SD card is idle
  if (PEEK(0xD680)&0x03) {
    POKE(0xD680,0x00);
    POKE(0xD680,0x01);
    while(PEEK(0xD680)&0x3) continue;
    usleep(500000L);
  }

  // Open the fonts
  for(i=0;i<NUM_FONTS;i++) {
    if (shopen(font_files[i],7,&fonts[i])) {
      printf("ERROR: Failed to open font '%s'\n", font_files[i]);
    return;
    }
  }

  // Try drawing a unicode glyph
  draw_glyph(0,0, FONT_UI, 0x0021,0x01);  
  
  return;
}
