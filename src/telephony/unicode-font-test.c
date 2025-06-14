#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"


#define NUM_FONTS 4
char *font_files[NUM_FONTS]={"NotoColorEmoji","NotoEmoji", "NotoSans", "Nokia Pixel Large"};
struct shared_resource fonts[NUM_FONTS];
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

unsigned char buffer[128];

unsigned char i;


void main(void)
{
  shared_resource_dir d;

  mega65_io_enable();

  // Make sure SD card is idle
  if (PEEK(0xD680)&0x03) {
    POKE(0xD680,0x00);
    POKE(0xD680,0x01);
    while(PEEK(0xD680)&0x3) continue;
    usleep(500000L);
  }

  for(i=0;i<NUM_FONTS;i++) {
    if (shopen(font_files[i],7,&fonts[i])) {
      printf("ERROR: Failed to open font '%s'\n", font_files[i]);
    return;
    }
  }

  return;
}
