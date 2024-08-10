// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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
