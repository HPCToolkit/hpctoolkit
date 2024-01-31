// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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
// File: elf-hash.c
//
// Purpose:
//    compute a crpytographic hash string for an elf binary
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "elf-hash.h"
#include "crypto-hash.h"



//******************************************************************************
// internal functions
//******************************************************************************

static int
elf_hash_compute
(
 const char *filename,
 char *hash_string,
 unsigned int hash_string_len
)
{
  struct stat statbuf;
  int status = -1;

  if (stat(filename, &statbuf) != -1) {
    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd != -1) {
      // for speed, hash at most FILE_MAX_HASH_LENGTH data
      size_t data_len = statbuf.st_size;

      void * ANYWHERE = 0;
      off_t NO_OFFSET = 0;
      void *data = mmap(ANYWHERE, data_len, PROT_READ, MAP_SHARED, fd,
                        NO_OFFSET);
      if (data) {
        status = crypto_compute_hash_string(data, data_len, hash_string,
                                            hash_string_len);
        munmap(data, data_len);
      }
      close(fd);
    }
  }

  return status;
}



//******************************************************************************
// interface functions
//******************************************************************************

char *
elf_hash
(
 const char *filename
)
{
  char *hash_string = (char *) malloc(CRYPTO_HASH_STRING_LENGTH);

  if (elf_hash_compute(filename, hash_string, CRYPTO_HASH_STRING_LENGTH) != 0) {
    free(hash_string);
    hash_string = 0;
  }

  return hash_string;
}



//******************************************************************************
// unit test
//******************************************************************************

#ifdef UNIT_TEST

#include <stdio.h>

#define FILENAME "/bin/ls"

int
main
(
 int argc,
 char **argv
)
{
  char *h = elf_hash(FILENAME);
  if (h) {
    printf("hash string: %s %s\n", h, FILENAME);
    free(h);
    return 0;
  }
  return -1;
}

#endif
