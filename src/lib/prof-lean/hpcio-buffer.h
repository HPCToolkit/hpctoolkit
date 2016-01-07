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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef prof_lean_hpcio_buffer
#define prof_lean_hpcio_buffer

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include "spinlock.h"


// Clients should treat the outbuf struct as opaque.

typedef struct hpcio_outbuf_s {
  uint32_t magic;
  void  *buf_start;
  size_t buf_size;
  size_t in_use;
  int  fd;
  int  flags;
  char use_lock;
  spinlock_t lock;
} hpcio_outbuf_t;


//***************************************************************************

// Flags for hpcio_outbuf_attach().

#define HPCIO_OUTBUF_LOCKED    0x1
#define HPCIO_OUTBUF_UNLOCKED  0x2

#if defined(__cplusplus)
extern "C" {
#endif

int
hpcio_outbuf_attach(hpcio_outbuf_t *outbuf /* out */, int fd,
		    void *buf_start, size_t buf_size, int flags);

ssize_t
hpcio_outbuf_write(hpcio_outbuf_t *outbuf, const void *data, size_t size);

int
hpcio_outbuf_flush(hpcio_outbuf_t *outbuf);

int
hpcio_outbuf_close(hpcio_outbuf_t *outbuf);

#if defined(__cplusplus)
}
#endif

#endif  // prof_lean_hpcio_buffer
