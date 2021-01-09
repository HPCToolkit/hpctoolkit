// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2020, Rice University
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
//   crypto-hash.c
//
// Purpose:
//   compute a cryptographic hash of a sequence of bytes. this is used
//   to name information presented to hpcrun in memory (e.g. a GPU binary) 
//   that needs to be saved for post-mortem analysis.
//
//***************************************************************************


//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>
#include <string.h>

#include <mbedtls/md5.h>  // MD5

#include "crypto-hash.h"



//*****************************************************************************
// macros
//*****************************************************************************

 

#define LOWER_NIBBLE_MASK 	(0x0f)
#define UPPER_NIBBLE(c) 	((c >> 4) & LOWER_NIBBLE_MASK)
#define LOWER_NIBBLE(c) 	(c  & LOWER_NIBBLE_MASK)

#define HEX_TO_ASCII(c) ((c > 9) ?  'a' + (c - 10) : ('0' + c))


//*****************************************************************************
// interface operations
//*****************************************************************************

//-----------------------------------------------------------------------------
// function: 
//   crypto_hash_compute
//
// arguments:
//   input:        
//     pointer to a vector of bytes that will be crytographically hashed
//   input_length:        
//     length in bytes of the input
//   hash:        
//     pointer to a vector of bytes of length >= crypto_hash_length()
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_hash_compute
(
  const unsigned char *input, 
  size_t input_length,
  unsigned char *hash,
  unsigned int hash_length
)
{
  if (hash_length > HASH_LENGTH) {
    // failure: caller not prepared to accept a hash of at least the length 
    // that we will provide
    return -1;
  }

  // zero the hash result
  memset(hash, 0, hash_length); 

  // compute an MD5 hash of input
  mbedtls_md5(input, input_length, hash);

  return 0;
}


//-----------------------------------------------------------------------------
// function: 
//   crypto_hash_length
//
// arguments: none
//
// return value:
//   number of bytes in the crytographic hash 
//-----------------------------------------------------------------------------
unsigned int
crypto_hash_length
(
  void
)
{
  return HASH_LENGTH;
}


//-----------------------------------------------------------------------------
// function: 
//   crypto_hash_to_hexstring
//
// arguments:
//   hash:        
//     pointer to crytographic hash computed by cryto_hash_compute
//   hash_string: 
//     pointer to character buffer where string equivalent of the hash code 
//     will be written
//   hash_string_length: 
//     length of the hash string must be > 2 * crypto_hash_length()
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_hash_to_hexstring
(
  unsigned char *hash,
  char *hash_string,
  unsigned int hash_string_length
)
{
  int hex_digits = HASH_LENGTH << 1;
  if (hash_string_length < hex_digits + 1) {
    return -1;
  }

  int chars = HASH_LENGTH;
  while (chars-- > 0) {
    unsigned char val_u = UPPER_NIBBLE(*hash); 
    *hash_string++ = HEX_TO_ASCII(val_u);

    unsigned char val_l = LOWER_NIBBLE(*hash);
    *hash_string++ = HEX_TO_ASCII(val_l);
    hash++;
  }
  *hash_string++ = 0;

  return 0;
}



//******************************************************************************
// unit test
//******************************************************************************

#if UNIT_TEST

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


//-----------------------------------------------------------------------------
// function: 
//   crypto_hash_self_test
//
// arguments:
//   verbose: if non-zero, it prints individual self test results
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_hash_self_test
(
  int verbose
)
{
  int status = mbedtls_md5_self_test(verbose);
  return status;
}


int
main(int argc, char **argv)
{
  int verbose = 1;
  int self_test_result = crypto_hash_self_test(verbose);

  if (self_test_result) {
     printf("%s crypto hash self test failed\n", argv[0]);
     exit(-1);
  } else {
     printf("%s crypto hash self test succeeded\n", argv[0]);
  }

  if (argc < 2) {
     printf("usage: %s <filename>\n", argv[0]);
     exit(-1);
  }

  const char *filename = argv[1];

  // check that the specified file is present
  struct stat statbuf;
  int stat_result = stat(filename, &statbuf);
  if (stat_result != 0) {
    printf("%s: stat failed %d\n", argv[0], stat_result);
    exit(-1);
  }
  size_t filesize = statbuf.st_size;

  char *filebuf; 
  filebuf = (char *) malloc(filesize);

  // read a file to hash
  FILE *file = fopen(filename, "r");
  size_t read_result = fread(filebuf, 1, filesize, file);
  if (read_result != filesize) {
    printf("%s: read failed. expected %ld bytes, got %ld bytes\n", 
	   argv[0], filesize, read_result);
    exit(-1);
  }

  // allocate space for hash
  int hash_len = crypto_hash_length();
  unsigned char *hash = (unsigned char *) malloc(hash_len);

  // compute hash
  int crypto_result = crypto_hash_compute(filebuf, filesize, hash, hash_len);
  if (crypto_result) {
    printf("%s: crypto_hash_compute failed. returned %d bytes\n", 
	   argv[0], filesize, crypto_result);
    exit(-1);
  }

  // allocate space for ASCII version of hash 
  int hash_strlen = hash_len << 1 + 1;
  char buffer[hash_strlen];

  // compute ASCII version of hash and dump it
  crypto_hash_to_hexstring(hash, buffer, hash_strlen);
  printf("hash string = %s\n", buffer);

  return 0;
}

#endif
