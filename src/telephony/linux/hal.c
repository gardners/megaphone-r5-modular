#include <stdio.h>

unsigned char sector_buffer[512];
#define SECTOR_BUFFER_ADDRESS ((unsigned long long) &sector_buffer[0])

void lfill(unsigned long long addr, unsigned char val, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lfill() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }
}

void lcopy(unsigned long long src, unsigned long long dest, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lcopy() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }
}

void write_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{
  unsigned int offset = (track-1)*(2*10*512) + (sector*512);

  if (drive_id>1) {
    fprintf(stderr,"FATAL: Illegal drive_id=%d in write_sector()\n",drive_id);
    exit(-1);
  }
  if (!drive_files[drive_id]) {
    fprintf(stderr,"FATAL: write_sector() called on drive %d, but it is not mounted.\n",drive_id);
    exit(-1);
  }

  if (fseek(offset,SEEK_SET,drive_files[drive_id])) {
    fprintf(stderr,"FATAL: write_sector(%d,%d,%d) failed to seek to offset 0x%06x.\n",drive_id,track,sector,offset);
    perror("write_sector()");
    exit(-1);
  }
  
  if (fwrite(sector_buffer,512,1,drive_files[drive_id])!=1) {
    fprintf(stderr,"FATAL: write_sector(%d,%d,%d) at offset 0x%06x failed.\n",drive_id,track,sector,offset);
    perror("write_sector()");
    exit(-1);
  }
}
