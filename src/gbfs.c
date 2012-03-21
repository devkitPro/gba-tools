/*

$Id: gbfs.c,v 1.1 2007-01-20 15:30:41 wntrmute Exp $

   create a GBFS file

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

$Log: not supported by cvs2svn $
Revision 1.3  2005/09/13 03:08:45  wntrmute
added header logging
close input files when done
reformatted for consistency


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

typedef unsigned short u16;
typedef unsigned long u32;  /* this needs to be changed on 64-bit systems */

#include "gbfs.h"

static const char GBFS_magic[] = "PinEightGBFS\r\n\032\n";

static const char help_text[] =
"Creates a GBFS archive.\n"
"usage: gbfs ARCHIVE [FILE...]\n";

GBFS_FILE header;
GBFS_ENTRY *entries;

//---------------------------------------------------------------------------------
unsigned long fcopy(FILE *dst, FILE *src) {
//---------------------------------------------------------------------------------
	char buf[16384];
	unsigned long sz = 0, this_sz;

	do {
		this_sz = fread(buf, 1, sizeof(buf), src);

		if(this_sz) {
			fwrite(buf, 1, this_sz, dst);  /* FIXME: need error checking */
			sz += this_sz;
		}
	} while(this_sz != 0);

	return sz;
}


/*---------------------------------------------------------------------------------
	fputi16()
	write a 16-bit integer in intel format to a file
---------------------------------------------------------------------------------*/
void fputi16(unsigned int in, FILE *f) {
//---------------------------------------------------------------------------------
	fputc(in, f);
	fputc(in >> 8, f);
}


/*---------------------------------------------------------------------------------
	fputi32()
	write a 32-bit integer in intel format to a file
---------------------------------------------------------------------------------*/
void fputi32(unsigned long in, FILE *f) {
//---------------------------------------------------------------------------------
	fputc(in, f);
	fputc(in >> 8, f);
	fputc(in >> 16, f);
	fputc(in >> 24, f);
}


/*---------------------------------------------------------------------------------
	namecmp()
	compares the first 24 bytes of a pair of strings.
	useful for sorting names.
---------------------------------------------------------------------------------*/
int namecmp(const void *a, const void *b) {
//---------------------------------------------------------------------------------
	return memcmp(a, b, 24);
}


//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	FILE *outfile;
	unsigned int arg = 2;
	unsigned int n_entries = 0;

	if(argc < 3) {
		fputs(help_text, stderr);
		return 1;
	}

	entries = malloc(argc * sizeof(GBFS_ENTRY));

	if(!entries) {
		perror("could not allocate memory for directory");
		return 1;
	}

 	memcpy(header.magic, GBFS_magic, sizeof(header.magic));
	header.dir_off = 32;
	n_entries = 0;

	/*	note that this creates some wasted space if n_entries turns out
		to be less than argc - 2 */
	header.total_len = header.dir_off + (argc - 2) * sizeof(GBFS_ENTRY);

	outfile = fopen("gbfs.$$$", "wb+");

	if(!outfile) {
		perror("could not open temporary file gbfs.$$$ for writing");
		return 1;
	}

	fseek(outfile, header.total_len, SEEK_SET);

	while(arg < argc) {

		FILE *infile;
		unsigned long flen;

		infile = fopen(argv[arg], "rb");

		if(!infile) {
			fprintf(stderr, "could not open %s: %s", argv[arg], strerror(errno));
			fclose(outfile);
			remove("gbfs.$$$");
			free(entries);
			return -1;
		}

		/* get current file offset */
		entries[n_entries].data_offset = ftell(outfile);

		/* copy the file into the data area */
		flen = fcopy(outfile, infile);
		entries[n_entries].len = flen;

		/* copy name */
		strncpy(	entries[n_entries].name,
					basename(argv[arg]),
					sizeof(entries[n_entries].name));

		/* diagnostic */
		{
			char nameout[32] = {0};

			strncpy(nameout, entries[n_entries].name, sizeof(entries[n_entries].name));
			printf("%10lu %s\n", flen, nameout);
		}

		/* pad file with 0's to para boundary */

		while(ftell(outfile) & 0x000f) {
			fputc(0, outfile);
		}

		// close the current input file
		fclose(infile);

		/* next file please */
		n_entries++;
		arg++;
	}

	header.total_len = ftell(outfile);
	rewind(outfile);

	/* sort directory by name */
	qsort(entries, n_entries, sizeof(entries[0]), namecmp);

	/* write header */
	fwrite(GBFS_magic, 16, 1, outfile);
	fputi32(header.total_len, outfile);
	fputi16(header.dir_off, outfile);
	fputi16(n_entries, outfile);


	/* write directory */
	fseek(outfile, header.dir_off, SEEK_SET);

	{
		unsigned int i;

		for(i = 0; i < n_entries; i++) {
			fwrite(entries[i].name, sizeof(entries[i].name), 1, outfile);
			fputi32(entries[i].len, outfile);
			fputi32(entries[i].data_offset, outfile);
		}
	}

	free(entries);
	fclose(outfile);

	remove(argv[1]);  /* some systems don't auto-remove the rename target */

	if(rename("gbfs.$$$", argv[1])) {
		fputs("could not rename gbfs.$$$ to ", stderr);
		perror(argv[1]);
		fputs("leaving finished archive in gbfs.$$$\n", stderr);
	}

	return 0;
}

/* The behavior of this program with respect to ordering of files

This information is subject to change.

Currently, the app writes the objects' data in order of appearance
on the command line, but it writes the directory in ABC order as
required by the format spec.

*/
