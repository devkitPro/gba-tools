/* ungbfs.c
   dump a GBFS file to the current directory

Copyright (C) 2002  Damian Yerrick

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to 
  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA  02111-1307, USA.
GNU licenses can be viewed online at http://www.gnu.org/copyleft/

In addition, as a special exception, Damian Yerrick gives
permission to link the code of this program with import libraries
distributed as part of the plug-in development tools published by
the vendor of any image manipulation program, and distribute
linked combinations including the two.  You must obey the GNU
General Public License in all respects for all of the code used
other than the plug-in interface.  If you modify this file, you
may extend this exception to your version of the file, but you
are not obligated to do so.  If you do not wish to do so, delete
this exception statement from your version.

Visit http://www.pineight.com/ for more information.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strequ(a,b) (!strcmp(a,b))

typedef unsigned short u16;
typedef unsigned long u32;

#include "gbfs.h"

/* fgeti16() ***************************
   read a 16-bit integer in intel format from a file
*/
u32 fgeti16(FILE *fp)
{
  return ((fgetc(fp) & 0x000000ff)      ) |
         ((fgetc(fp) & 0x000000ff) <<  8);
}


/* fgeti32() ***************************
   read a 32-bit integer in intel format from a file
*/
u32 fgeti32(FILE *fp)
{
  return ((fgetc(fp) & 0x000000ff)      ) |
         ((fgetc(fp) & 0x000000ff) <<  8) |
         ((fgetc(fp) & 0x000000ff) << 16) |
         ((fgetc(fp) & 0x000000ff) << 24);
}


/* fncpy() *****************************
   copy n bytes from one file to the other
*/
void fncpy(FILE *dst, FILE *src, unsigned long n)
{
  char buf[16384];

  while(n > 0)
  {
    unsigned long n_copy = n;

    if(n_copy > 16384)
      n_copy = 16384;
    fread(buf, 1, n_copy, src);
    fwrite(buf, 1, n_copy, dst);
    n -= n_copy;
  }
}


int main(int argc, char **argv)
{
  FILE *fp;
  char filename[32] = {0};
  u32 dir_off, dir_nmemb;
  unsigned int i;

  if(argc != 2 || strequ("-h", argv[1]) || strequ("--help", argv[1]))
  {
    fputs("dumps the objects in a gbfs file to separate files\n"
          "syntax: ungbfs FILE\n", stderr);
    return 1;
  }

  fp = fopen(argv[1], "rb");
  if(!fp)
  {
    fputs("could not open ", stderr);
    perror(argv[1]);
    return 1;
  }

  /* read pertinent header fields */
  fseek(fp, 20, SEEK_SET);
  dir_off = fgeti16(fp);
  dir_nmemb = fgeti16(fp);

  for(i = 0; i < dir_nmemb; i++)
  {
    unsigned long len, off;
    FILE *outfile;

    fseek(fp, dir_off + 32 * i, SEEK_SET);
    fread(filename, 24, 1, fp);
    len = fgeti32(fp);
    off = fgeti32(fp);
    printf("%10lu %s\n", (unsigned long)len, filename);

    outfile = fopen(filename, "wb");
    if(!outfile)
    {
      fputs("could not open ", stderr);
      perror(filename);
      fclose(fp);
      return 1;
    }
    fseek(fp, off, SEEK_SET);
    fncpy(outfile, fp, len);
    fclose(outfile);
  }

  fclose(fp);
  return 0;
}
