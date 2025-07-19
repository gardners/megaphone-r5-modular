// Load HAL
#include "includes.h"

/*
  Provision the entire MEGAphone directory structure and phone state.

  Currently works only on Linux, not natively on the MEGA65, due to the
  lack of working MKDIR hypervisor trap.
*/

char to_hex(unsigned char v)
{
  if (v<0xa) return v+'0';
  if (v>0xf) return 0;
  return 'A'+(v-0xa);
}

int main(int argc,char **argv)
{
  #define MAX_CONTACTS 1600
  unsigned short contact;
  char hex[2];
  
  hal_init();
  mega65_mkdir("PHONE");
  mega65_chdir("PHONE");
  mega65_mkdir("THREADS");

  /* For fast traversal (i.e., short directory listings) we
     create 4 layers deep of 0-F directory names.
     That allows for 65,536 contacts. Given that we only
     support 1,560 contacts in other parts of the system for
     now, that's safe. I could have limited it to 3 layers and
     4,096 contacts, but we do have a mechanism to increase the
     contact count to ~15,600, so 4 layers is just safer.

     Then inside each of those directories we need a MESSAGES.D81
     and MSGINDEX.D81 for indexing the message D81s (providing quick
     search facility via an index of diphtongs occurring in the
     messages).

     While we'll create the 4-level directory structure, we will
     only populate the first 1,580 with those files, so that we don't
     eat up an insane amount of SD card. It will already be a few GB,
     as it is.
  */

  for(contact=0;contact<MAX_CONTACTS;contact++) {

    printf("Creating contact %d\n",contact);
    
    // For compatibiity with MEGA65 HAL, we can't
    // CD .. or use relative paths. So we have to
    // slightly inefficiently go all the way back to the root
    // directory and make the next directory.
    // This will be SLOW -- at least until we get hardware
    // accelerated FAT filesystem stuff and/or the SD card
    // cache, as there will be lots of directory traversals
    // going on.  Such is life.
    mega65_cdroot();
    mega65_chdir("PHONES");
    mega65_chdir("THREADS");
    hex[0]=to_hex(contact>>12);
    hex[1]=0;
    mega65_mkdir(hex);
    mega65_chdir(hex);
    hex[0]=to_hex(contact>>8);
    hex[1]=0;
    mega65_mkdir(hex);
    mega65_chdir(hex);
    hex[0]=to_hex(contact>>4);
    hex[1]=0;
    mega65_mkdir(hex);
    mega65_chdir(hex);
    hex[0]=to_hex(contact>>0);
    hex[1]=0;
    mega65_mkdir(hex);
    mega65_chdir(hex);

    create_d81("MESSAGES.D81");
    create_d81("MSGINDEX.D81");
    mount_d81(0,"MESSAGES.D81");
    format_image_fully_allocated(0,"MESSAGES");
    mount_d81(0,"MSGINDEX.D81");
    format_image_fully_allocated(0,"MESSAGE INDEX");
    
  }
     
}
