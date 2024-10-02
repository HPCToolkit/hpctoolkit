// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#include "md5.h"
#include "crypto-hash.h"



//*****************************************************************************
// macros
//*****************************************************************************



#define LOWER_NIBBLE_MASK       (0x0f)
#define UPPER_NIBBLE(c)         ((c >> 4) & LOWER_NIBBLE_MASK)
#define LOWER_NIBBLE(c)         (c  & LOWER_NIBBLE_MASK)

#define HEX_TO_ASCII(c) ((c > 9) ?  'a' + (c - 10) : ('0' + c))


//*****************************************************************************
// interface operations
//*****************************************************************************

//-----------------------------------------------------------------------------
// function:
//   crypto_compute_hash
//
// arguments:
//   input:
//     pointer to a vector of bytes that will be cryptographically hashed
//   input_length:
//     length in bytes of the input
//   hash:
//     pointer to a vector of bytes of length >= CRYPTO_HASH_LENGTH
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_compute_hash
(
  const void *data,
  size_t data_bytes,
  unsigned char *hash,
  unsigned int hash_length
)
{
  struct md5_context context;
  struct md5_digest digest;

  if (hash_length < CRYPTO_HASH_LENGTH) {
    // failure: caller not prepared to accept a hash of at least the length
    // that we will provide
    return -1;
  }

  // zero the hash result
  memset(hash, 0, hash_length);

  // compute an MD5 hash of input
  md5_init(&context);
  md5_update(&context, data, (unsigned int) data_bytes);
  md5_finalize(&context, &digest);

  memcpy(hash, &digest, CRYPTO_HASH_LENGTH);

  return 0;
}


//-----------------------------------------------------------------------------
// function:
//   crypto_hash_to_hexstring
//
// arguments:
//   hash:
//     pointer to cryptographic hash computed by cryto_hash_compute
//   hash_string:
//     pointer to character buffer where string equivalent of the hash code
//     will be written
//   hash_string_length:
//     length of the hash string must be >= CRYPTO_HASH_STRING_LENGTH
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_hash_to_hexstring
(
  const unsigned char *hash,
  char *hash_string,
  unsigned int hash_string_length
)
{
  if (hash_string_length < CRYPTO_HASH_STRING_LENGTH) {
    return -1;
  }

  int chars = CRYPTO_HASH_LENGTH;
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


//-----------------------------------------------------------------------------
// function:
//   crypto_compute_hash_string
//
// arguments:
//   data:
//     pointer to data to hash
//   data_size:
//     length of data in bytes
//   hash_string:
//     pointer to result string from hashing data bytes
//   hash_string_length:
//     length of the hash string must be >= CRYPTO_HASH_STRING_LENGTH
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_compute_hash_string
(
 const void *data,
 size_t data_bytes,
 char *hash_string,
 unsigned int hash_string_length
)
{
  if (hash_string_length < CRYPTO_HASH_STRING_LENGTH) {
    return -1;
  }

  // Compute hash for data
  unsigned char hash[CRYPTO_HASH_LENGTH];
  crypto_compute_hash(data, data_bytes, hash, CRYPTO_HASH_LENGTH);

  // Turn hash into string
  crypto_hash_to_hexstring(hash, hash_string, CRYPTO_HASH_STRING_LENGTH);

  return 0;
}
