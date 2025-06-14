#include <stdio.h>
#include <string.h>

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

struct shared_resource dirent;
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
  
  printf("SD card status = 0x%02x\n",PEEK(0xD680));

  printf("Opening SHRES dir\n");
  d = shdopen(); 
  
  if (d==0xffff) {
    fprintf(stdout,"ERROR: Failed shdopen() failed.\n");
  printf("SD card status = 0x%02x\n",PEEK(0xD680));
    return;
  }

  printf("Now scanning directory.\n");
  
  while (!shdread(required_flags, &d,&dirent)) {
    printf("File: '%s'\n",dirent.name);

    if (dirent.name[0]==0x52||dirent.name[0]==0x72) {
      unsigned int b = 0;
      printf("Found text file\n");
      while(b = shread(buffer,128,&dirent)) {
	printf(">>>");
	for(i=0;i<b;i++) putchar(buffer[i]);
	printf("<<<");
      }
    }
    
  }

  return;
}
