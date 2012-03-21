/* insgbfs.c
   insert a GBFS file into a ROM

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


#define SIG_LEN 18

const char sig[SIG_LEN] = "PinEightGBFSSpace-";

static const char help_text[] =
"Inserts a GBFS file (or any other file) into a GBFS_SPACE (identified by\n"
"symbol name) in a ROM.\n\n"
"usage: insgbfs SOURCEFILE ROMFILE SYMNAME\n"
"example: insgbfs samples.gbfs marco.gba samples\n";


unsigned long fcopy(FILE *dst, FILE *src)
{
  char buf[16384];
  unsigned long sz = 0, this_sz;

  do {
    this_sz = fread(buf, 1, sizeof(buf), src);
    if(this_sz)
    {
      fwrite(buf, 1, this_sz, dst);  /* FIXME: need error checking */
      sz += this_sz;
    }
  } while(this_sz != 0);

  return sz;
}



/* find_signature() ********************
   Finds the next GBFS_SPACE signature and places the file pointer
   at its end.  Returns 0 for success or nonzero for failure.
*/
int find_signature(FILE *fp)
{
  size_t src_offset = 0;

  while(src_offset < SIG_LEN)
  {
    int c = fgetc(fp);

    if(c == EOF)
      return EOF;
    else if(c == sig[src_offset])
    {
      src_offset++;
    }
  }

  return 0;
}


/* find_signature_named() **************
   Finds a signature of the format
   PinEightGBFSSpace-sym-123
   where sym matches name.
   Positions the file pointer at the beginning of the sig.
   Returns the number after the sig, or <0 for not found.
*/
int find_signature_named(const char *name, FILE *fp)
{
  int is_eof, found = 0;
  int got_k = -1;

  for(is_eof = find_signature(fp);
      !found && !is_eof;
      found || (is_eof = find_signature(fp)))
  {
    const char *name_chr = name;
    size_t sig_offset = ftell(fp) - SIG_LEN;

    /* search for the name */
    while(*name_chr && *name_chr == fgetc(fp))
      name_chr++;

    if(*name_chr)  /* if it didn't make it all the way through */
    {
      fprintf(stderr, "didn't match at %lu\n", (unsigned long)sig_offset);
      continue;
    }

    /* if the name doesn't end there */
    if(fgetc(fp) != '-')
    {
      fprintf(stderr, "too long at %lu\n", (unsigned long)sig_offset);
      continue;
    }

    found = 1;
    {
      char buf[16];

      fread(buf, 1, sizeof(buf), fp);
      fseek(fp, sig_offset, SEEK_SET);
      got_k = strtoul(buf, NULL, 10);
      fprintf(stderr, "match at %lu, size %d\n",
                      (unsigned long)sig_offset,
                      got_k);
    }
  }

  return got_k;
}



int main(int argc, const char **argv)
{
  int found;
  FILE *infile, *romfile;
  size_t infile_size;

  if(argc < 4)
  {
    fputs(help_text, stderr);
    return EXIT_FAILURE;
  }

  romfile = fopen(argv[2], "rb+");
  if(!romfile)
  {
    fputs("insgbfs could not open ", stderr);
    perror(argv[2]);
    return EXIT_FAILURE;
  }

  found = find_signature_named(argv[3], romfile);
  if(found < 0)
  {
    fprintf(stderr, "insgbfs could not find symbol '%s' in file '%s'\n",
                    argv[3], argv[2]);
    fclose(romfile);
    return EXIT_FAILURE;
  }

  infile = fopen(argv[1], "rb");
  if(!infile)
  {
    fputs("insgbfs could not open ", stderr);
    perror(argv[1]);
    fclose(romfile);
    return EXIT_FAILURE;
  }

  fseek(infile, 0, SEEK_END);
  infile_size = ftell(infile);
  rewind(infile);
  if(infile_size > found * 1024)
  {
    fprintf(stderr, "insgbfs could not insert '%s' of %lu KB into a %d KB space in file '%s'\n",
                    argv[1],
                    (unsigned long)((infile_size - 1) / 1024 + 1),
                    found,
                    argv[2]);
    fclose(infile);
    fclose(romfile);
    return EXIT_FAILURE;
  }

  fcopy(romfile, infile);
  fclose(romfile);
  return EXIT_SUCCESS;
}

