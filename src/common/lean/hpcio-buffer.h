// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef prof_lean_hpcio_buffer
#define prof_lean_hpcio_buffer

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

#include "allocator.h"

// opaque type

typedef struct hpcio_outbuf_s hpcio_outbuf_t;

//***************************************************************************

// Flags for hpcio_outbuf_attach().

#define HPCIO_OUTBUF_LOCKED    0x1
#define HPCIO_OUTBUF_UNLOCKED  0x2

#if defined(__cplusplus)
extern "C" {
#endif

int
hpcio_outbuf_attach
(
  hpcio_outbuf_t **outbuf /* out */,
  int fd,
  void *buf_start,
  size_t buf_size,
  int flags,
  allocator_t alloc
);


ssize_t
hpcio_outbuf_write
(
  hpcio_outbuf_t *outbuf,
  const void *data,
  size_t size
);


int
hpcio_outbuf_flush
(
  hpcio_outbuf_t *outbuf
);


int
hpcio_outbuf_close
(
  hpcio_outbuf_t **outbuf
);


#if defined(__cplusplus)
}
#endif

#endif  // prof_lean_hpcio_buffer
