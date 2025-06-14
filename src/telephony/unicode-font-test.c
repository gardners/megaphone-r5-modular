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
  // 16-bit text mode 
  POKE(0xD054,0x05);

  // PAL
  POKE(0xD06f,0x00);
  
  // Retract borders to be 1px
  POKE(0xD05C,0x3B);
  POKE(0xD048,0x3B); POKE(0xD049,0x00);
  POKE(0xD04A,0x1c); POKE(0xD04B,0x02);  

  // H640 + fast CPU
  POKE(0xD031,0xc0);  
  
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
  POKE(0xD04E,0x39); POKE(0xD04F,0x00);
  
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

// 128KB buffer for 128KB / 256 bytes per glyph = 512 unique unicode glyphs on screen at once
#define GLYPH_DATA_START 0x40000
#define GLYPH_CACHE_SIZE 512
#define BYTES_PER_GLYPH 256
unsigned long cached_codepoints[GLYPH_CACHE_SIZE];
unsigned char cached_fontnums[GLYPH_CACHE_SIZE];
unsigned char glyph_buffer[BYTES_PER_GLYPH];

void reset_glyph_cache(void)
{
  lfill(GLYPH_DATA_START,0x00,GLYPH_CACHE_SIZE * BYTES_PER_GLYPH);
  lfill((unsigned long)cached_codepoints,0x00,GLYPH_CACHE_SIZE*sizeof(unsigned long));
}

void load_glyph(int font, unsigned long codepoint, unsigned int cache_slot)
{
  shseek(&fonts[font],codepoint<<8,SEEK_SET);
  shread(glyph_buffer,256,&fonts[font]);
  // XXX Extract glyph flags etc
  lcopy((unsigned long)glyph_buffer,GLYPH_DATA_START + ((unsigned long)cache_slot<<8), BYTES_PER_GLYPH);
  cached_codepoints[cache_slot]=codepoint;
  cached_fontnums[cache_slot]=font;
}

void draw_glyph(int x, int y, int font, unsigned long codepoint)
{
  unsigned int i;
  for(i=0;i<GLYPH_CACHE_SIZE;i++) {
    if (!cached_codepoints[i]) break;
    if (cached_codepoints[i]==codepoint&&cached_fontnums[i]==font) break;
  }
  if (cached_codepoints[i]!=codepoint) {
    load_glyph(font, codepoint, i);
  }

}

void main(void)
{
  shared_resource_dir d;

  mega65_io_enable();

  screen_setup();
  screen_clear();
  
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

  
  
  return;
}
