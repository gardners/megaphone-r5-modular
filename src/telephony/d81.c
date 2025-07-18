// Include file for either MEGA65 or Linux compilation
#include "includes.h"

unsigned char header_template[32]={
  40,3, // First directory sector
  0x44, // DOS Version ('D')
  // Disk label, 0xa0 padded
  0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,
  0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,
  // 2x 0xa0 padding bytes
  0xa0,0xa0,
  // Disk ID
  '6','5',
  // 0xa0 padding
  0xa0,
  // DOS type ('3D')
  '3',0x44,
  // 4x 0xa0 padding
  0xa0,0xa0,0xa0,0xa0,
  // 1x 0x00 padding
  0x00};

unsigned char bam_template[8]={
  40,2, // Sector number of next BAM sector
  0x44, // DOS Version
  0xbb, // 1s complement of version (i.e., 0xff XOR 0x44 = 0xbb)
  '6','5', // Disk ID
  0xc0, // I/O byte: bit 7 = verify on, bit 6 = check sector header CRC
  0x00 // Auto-boot flag
};

// We only care about the 19 bytes of relevance for non-REL non-GEOS files
unsigned char directory_entry_template[20]={
  0xC1, // SEQ< (read-only)
  1,0, // Starts on track 1 (track 0 means unused, i.e., track numbers are 1 -- 80), sector 0
  // Filename (PETSCII, 0xa0 padded)
  0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,
  0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0,0xa0
};

unsigned char all_sectors_filename[16]={
  'A','L','L',' ','S','E','C','T','O','R','S',
  0xa0,0xa0,0xa0,0xa0,0xa0
};

/*
  Format a D81 disk image to contain a single file that
  occupies the entire disk. Allocate 24 directory entries.
*/
void format_image_fully_allocated(char drive_id,char *header)  
{
  char i,j;

  /* Header + BAM Sector 1
     ---------------------------------------- */
  
  // Erase sector buffer
  lfill(SECTOR_BUFFER_ADDRESS,0x00,512);
  // Clear header name
  lfill(&header_template[4],0xa0,16);
  // Set header name
  for(i=0;(i<16) && (header[i]);i++) {
    header_template[4+i] = ascii_to_petscii(header[i]);
  }
  lcopy(header_template,SECTOR_BUFFER_ADDRESS, sizeof(header_template));

  // BAM is easy: Just a valid header, and leave all bitmap bytes $00, to
  // indicate that there is no free space on the disk.
  lcopy(bam_template,SECTOR_BUFFER_ADDRESS + 0x100,sizeof(bam_template));
    
  write_sector(drive_id,40,0);

  /* BAM Sector 2 + First directory sector */

  // Erase sector buffer
  lfill(SECTOR_BUFFER_ADDRESS,0x00,512);

  // BAM Sector 2 is the same as BAM Sector 1, except that as it is the last BAM Sector,
  // the link to the next sector is $00,$FF, to indicate end of "file", and that it uses
  // all the bytes.
  lcopy(&bam_template[2],SECTOR_BUFFER_ADDRESS + 0x002,sizeof(bam_template));
  lpoke(SECTOR_BUFFER_ADDRESS+0,0x00);
  lpoke(SECTOR_BUFFER_ADDRESS+1,0xFF);

  // We assume that we always need 16 directory entries, so setup sector link to
  // track 40 sector 4.
  lpoke(SECTOR_BUFFER_ADDRESS+0x100,40);
  lpoke(SECTOR_BUFFER_ADDRESS+0x101,4);

  // Set filename of "all sectors" file
  lcopy(all_sectors_filename,&directory_entry_template[3],16);
  // Create directory entry
  lcopy(directory_entry_template,SECTOR_BUFFER_ADDRESS + 0x102, sizeof(directory_entry_template));
  // Clean up after scribbling over the directory entry template
  lfill(&directory_entry_template[3],0xa0,16);
  
  write_sector(drive_id,40,1);
  
  /* Directory sectors 2 and 3 */

  // Erase sector buffer
  lfill(SECTOR_BUFFER_ADDRESS,0x00,512);

  // Directory sector links
  lpoke(SECTOR_BUFFER_ADDRESS+0x000,40);
  lpoke(SECTOR_BUFFER_ADDRESS+0x001,5);
  lpoke(SECTOR_BUFFER_ADDRESS+0x100,0);
  lpoke(SECTOR_BUFFER_ADDRESS+0x101,0xff);

  write_sector(drive_id,40,2);

  /* Iterate through the entire disk and write sector links */

  // Tracks
  for(i=1;i<=80;i++) {
    // Don't touch the directory sector
    if (i==0x40) continue;

    // Physical sectors
    for(j=0;j<20;j++) {
      
      // Next sector links

      // First half always links to 2nd half of physical sector
      lpoke(SECTOR_BUFFER_ADDRESS+0x000,i);
      lpoke(SECTOR_BUFFER_ADDRESS+0x001,j*2+1);

      // Second half points to next physical sector, or to first
      // sector of next track (or track 41 if we are on track 39,
      // so that we skip the directory).
      if (j<20) {
	lpoke(SECTOR_BUFFER_ADDRESS+0x100,i);
	lpoke(SECTOR_BUFFER_ADDRESS+0x101,j+1);
      } else {
	lpoke(SECTOR_BUFFER_ADDRESS+0x100, (i==39)?41:(i+1));
	lpoke(SECTOR_BUFFER_ADDRESS+0x101,0);
      }

      write_sector(drive_id,i,j);
      
    }
  }
  
}
