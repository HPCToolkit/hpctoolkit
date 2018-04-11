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
// Copyright ((c)) 2002-2017, Rice University
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
