#include <stdio.h>
#include <string.h>

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

struct shared_resource dirent;
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

unsigned char buffer[128];

unsigned char i;

char filename_ascii[]={'r','e','a','d','m','e','.',0x6d,0x64,0};

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

  if (shopen(filename_ascii,0,&dirent)) {
    printf("ERROR: Failed to open README.md\n");
    return;
  }

  {
    unsigned int b = 0;
    printf("Found text file\n");
    while(b = shread(buffer,128,&dirent)) {
      for(i=0;i<b;i++) {
	if (buffer[i]==0x0a) putchar(0x0d); else putchar(buffer[i]);
      }
      
    }
  }

  return;
}
