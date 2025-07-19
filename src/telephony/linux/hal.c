#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

unsigned char sector_buffer[512];

#define MAX_DRIVES 2
FILE *drive_files[MAX_DRIVES]={NULL};

#define PATH_LEN 2048
char working_directory[PATH_LEN];

void hal_init(void)
{
  if (!getcwd(working_directory,PATH_LEN)) {
    fprintf(stderr,"FATAL: Failed to read current working directory in hal_init()\n");
    perror("getcwd()");
    exit(-1);
  }
}

char mega65_mkdir(char *dir)
{
  char cwd[2048];
  fprintf(stderr,"INFO: Making directory '%s' in '%s'\n",
	  dir,getcwd(cwd,sizeof(cwd)));
  return mkdir(dir,0750);
}

char mega65_cdroot(void)
{
  char cwd[2048];
  fprintf(stderr,"INFO: CDROOT: Changing directory to '%s'\n",
	  working_directory);
  return chdir(working_directory);
}

char mega65_chdir(char *dir)
{
  char cwd[2048];
  fprintf(stderr,"INFO: Changing directory to '%s' in '%s'\n",
	  dir,getcwd(cwd,sizeof(cwd)));
  int r= chdir(dir);
  if (r) perror("chdir()");
  return r;
}


void lpoke(unsigned long long addr, unsigned char val)
{
  unsigned char *a = (unsigned char *)addr;
  a[0]=val;
}


void lfill(unsigned long long addr, unsigned char val, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lfill() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }

  memset((unsigned char *)addr,val,len);
  
}

void lcopy(unsigned long long src, unsigned long long dest, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lcopy() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }

  memmove((unsigned char *)dest,(unsigned char *)src,len);
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

  if (fseek(drive_files[drive_id],offset,SEEK_SET)) {
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

char mount_d81(char *filename, unsigned char drive_id)
{
  if (drive_id >= MAX_DRIVES) {
    fprintf(stderr,"ERROR: Attempted to mount a disk image to drive %d (must be 0 -- %d)\n",
	    drive_id, MAX_DRIVES-1);
    return -1;
  }
  if (drive_files[drive_id]) { fclose(drive_files[drive_id]); drive_files[drive_id]=NULL; }

  drive_files[drive_id]=fopen(filename,"rb+");
  if (!drive_files[drive_id]) {
    fprintf(stderr,"ERROR: Failed to mount '%s' as drive %d\n",filename,drive_id);
    perror("fopen()");
    return -1;
  }

  fprintf(stderr,"INFO: Disk image '%s' mounted as drive %d\n",filename,drive_id);
  
  return 0;
}

char create_d81(char *filename)
{
  FILE *f=fopen(filename,"rb");
  if (f) {
    fprintf(stderr,"ERROR: Disk image '%s' already exists.\n",filename);
    fclose(f);
    return -1;
  }

  f=fopen(filename,"wb");
  if (!f) {
    fprintf(stderr,"ERROR: Failed to create disk image '%s'\n",filename);
    perror("fopen()");
    return -1;
  }

  if (ftruncate(fileno(f), 800*1024)) {  
    fprintf(stderr,"ERROR: Failed to set size of newly created disk image '%s'\n",filename);
    perror("ftruncate()");
    return -1;
  }


  fclose(f);
  
  return 0;
}
