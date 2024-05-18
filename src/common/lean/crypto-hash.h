// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   crypto-hash.h
//
// Purpose:
//   compute a cryptographic hash of a sequence of bytes. this is used
//   to name information presented to hpcrun in memory (e.g. a GPU binary)
//   that needs to be saved for post-mortem analysis.
//
//***************************************************************************

#ifndef _HPCTOOLKIT_CRYPTO_HASH_H_
#define _HPCTOOLKIT_CRYPTO_HASH_H_

#define MD5_HASH_NBYTES 16

#define CRYPTO_HASH_LENGTH MD5_HASH_NBYTES

#define CRYPTO_HASH_STRING_LENGTH (1 + (CRYPTO_HASH_LENGTH << 1))

//*****************************************************************************
// interface operations
//*****************************************************************************

#if defined(__cplusplus)
extern "C" {
#endif


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
//     pointer to a vector of bytes of length >= crypto_hash_length()
//
// return value:
//   0: success
//   non-zero: failure
//-----------------------------------------------------------------------------
int
crypto_compute_hash
(
  const void *input,
  size_t input_bytes,
  unsigned char *hash,
  unsigned int hash_length
);


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
//     length of the hash string must be > 2 * crypto_hash_length()
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
);


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
);


#if defined(__cplusplus)
}
#endif

#endif
