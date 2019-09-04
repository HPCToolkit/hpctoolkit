// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#include "compress.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"


#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

/* Compress from file source to file dest until EOF on source.
It returns:
     COMPRESS_OK on success,
     COMPRESS_FAIL if the inflate data is invalid or the version is
       incorrect,
     COMPRESS_IO_ERROR is there is an error reading or writing the file

   The compression level must be Z_DEFAULT_COMPRESSION,
   or between 0 and 9: 1 gives best speed, 9 gives best compression,
   0 gives no compression at all (the input data is simply copied a
   block at a time). Z_DEFAULT_COMPRESSION requests a default compromise
   between speed and compression (currently equivalent to level 6).
   */
enum compress_e
compress_deflate(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return COMPRESS_FAIL;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return COMPRESS_IO_ERROR;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return COMPRESS_OK;
}


/* Decompress from file source to file dest until stream ends or EOF.
   It returns:
     COMPRESS_OK on success,
     COMPRESS_FAIL if the deflate data is invalid or the version is
       incorrect,
     COMPRESS_IO_ERROR is there is an error reading or writing the file
     COMPRESS_NONE if decompression is not needed.
 */
enum compress_e
compress_inflate(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return COMPRESS_FAIL;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return COMPRESS_IO_ERROR;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return COMPRESS_FAIL;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return COMPRESS_IO_ERROR;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? COMPRESS_OK : COMPRESS_IO_ERROR;
}

#ifdef __UNIT_TEST_COMPRESS__
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
int main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("syntax: %s input_compressed_file  output_file\n", argv[0]);
    exit(0);
  }
  FILE *fp_in  = fopen(argv[1], "r");
  FILE *fp_out = fopen(argv[2], "wx");

  if (fp_in == NULL || fp_out == NULL) {
    perror("fail to open file:");
  }
  // test 1: compressing file
  int ret = compress_deflate(fp_in, fp_out, 1);
  if (ret != COMPRESS_OK) {
    printf("cannot compress %s into %s\n", argv[1], argv[2]);
    perror("compress fail:");
  }
  fclose(fp_in);
  fclose(fp_out);

  // test 2: decompressing file
  fp_out = fopen(argv[2], "r");
  FILE *fp_def = tmpfile();
  ret = compress_inflate(fp_out, fp_def);
  if (ret != COMPRESS_OK) {
    printf("cannot decompress %s\n", argv[2]);
    perror("compress fail:");
  }

  // testing the output
  char buffer[11];
  fseek(fp_def, 0, SEEK_SET);
  fgets(buffer, 10, fp_def);
  buffer[10] = '\0';
  printf("file: '%s'\n", buffer);

  fclose(fp_out);
  fclose(fp_def);

}
#endif
