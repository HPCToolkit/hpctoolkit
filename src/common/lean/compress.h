// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

#ifndef HPCTOOLKIT_COMPRESS_H
#define HPCTOOLKIT_COMPRESS_H

#include <stdio.h>


#ifdef __cplusplus
extern "C"
{
#endif

#define COMPRESSION_LEVEL_DEFAULT 6

enum compress_e {
  COMPRESS_OK, COMPRESS_FAIL, COMPRESS_IO_ERROR, COMPRESS_NONE
};

/* Compress from file source to file dest until EOF on source.
It returns:
     COMPRESS_OK on success,
     COMPRESS_FAIL if the inflate data is invalid or the version is
       incorrect,
     COMPRESS_IO_ERROR is there is an error reading or writing the file
     COMPRESS_NONE if compression is not needed.

   The compression level must be COMPRESSION_LEVEL_DEFAULT,
   or between 0 and 9: 1 gives best speed, 9 gives best compression,
   0 gives no compression at all (the input data is simply copied a
   block at a time). Z_DEFAULT_COMPRESSION requests a default compromise
   between speed and compression (currently equivalent to level 6).
 */
enum compress_e
compress_deflate(FILE *source, FILE *dest, int level);

/* Decompress from file source to file dest until stream ends or EOF.
   It returns:
     COMPRESS_OK on success,
     COMPRESS_FAIL if the deflate data is invalid or the version is
       incorrect,
     COMPRESS_IO_ERROR is there is an error reading or writing the file
     COMPRESS_NONE if decompression is not needed.
 */
enum compress_e
compress_inflate(FILE *source, FILE *dest);


#ifdef __cplusplus
}
#endif


#endif // HPCTOOLKIT_COMPRESS_H
