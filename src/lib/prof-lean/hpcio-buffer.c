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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
// This file implements a simple output buffer for hpcrun.  The main
// reason for writing our own buffer is to have something that is safe
// inside signal handlers and doesn't interact with the standard I/O
// streams library, especially fflush().
//
// We don't need to reimplement all of stdio, just enough to meet the
// special needs of writing hpcrun and hpctrace files from hpcrun.
// Programs like hpcstruct and hpcprof should continue to use the
// stdio library.
//
// Note: this file should be careful only to use functions that are
// safe inside signal handlers.  In particular, don't use malloc or
// functions that call malloc, and don't use the stdio library.
//
// Note: it's important for the client to call close (or at least
// flush) at the end of the process.  We don't automatically gain
// control at the end of the process, so there is no way to auto close
// the buffers.
//
// Deserves further study: the best way to handle errors from write().
//
//***************************************************************************

//************************* System Include Files ****************************

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>


//*************************** User Include Files ****************************

#include "hpcfmt.h"
#include "hpcio-buffer.h"
#include "spinlock.h"
#include <include/min-max.h>

#define HPCIO_OUTBUF_MAGIC  0x494F4246


//*************************** Private Functions *****************************

// Try to write() the entire outbuf.
//
// Returns: HPCFMT_OK if the entire buffer was successfully written,
// else HPCFMT_ERR.
//
static int
outbuf_flush_buffer(hpcio_outbuf_t *outbuf)
{
  ssize_t amt_done, ret;

  amt_done = 0;
  while (amt_done < outbuf->in_use) {
    errno = 0;
    ret = write(outbuf->fd, outbuf->buf_start + amt_done,
		outbuf->in_use - amt_done);

    // Check for short writes.  Note: EINTR is not failure.
    if (ret > 0 || (ret == 0 && errno == EINTR)) {
      // progress, continue
      amt_done += ret;
    }
    else {
      // hard failure
      // FIXME: should do better than copy to begin of buffer.
      // However, this case is rare, it probably will continue to fail
      // and I want to rethink this case anyway.  So, this will do for
      // now. (krentel)
      if (amt_done > 0) {
	memmove(outbuf->buf_start, outbuf->buf_start + amt_done,
		outbuf->in_use - amt_done);
	outbuf->in_use = amt_done;
      }
      return HPCFMT_ERR;
    }
  }

  // entire buffer was successfully written
  outbuf->in_use = 0;
  return HPCFMT_OK;
}


//*************************** Interface Functions ***************************

// Attach the file descriptor to the buffer, initialize and fill in
// the outbuf struct.  The client supplies the buffer and fd.
//
// Returns: HPCFMT_OK on success, else HPCFMT_ERR.
//
int
hpcio_outbuf_attach(hpcio_outbuf_t *outbuf /* out */, int fd,
		    void *buf_start, size_t buf_size, int flags)
{
  // Attach fills in the outbuf struct, so there is no magic number
  // and locks don't exist.
  if (outbuf == NULL || fd < 0 || buf_start == NULL || buf_size == 0) {
    return HPCFMT_ERR;
  }

  outbuf->magic = HPCIO_OUTBUF_MAGIC;
  outbuf->buf_start = buf_start;
  outbuf->buf_size = buf_size;
  outbuf->in_use = 0;
  outbuf->fd = fd;
  outbuf->flags = flags;
  outbuf->use_lock = (flags & HPCIO_OUTBUF_LOCKED);
  spinlock_unlock(&outbuf->lock);

  return HPCFMT_OK;
}


// Copy data to the outbuf and flush if necessary.
//
// Returns: number of bytes copied, or else -1 on bad buffer.
//
ssize_t
hpcio_outbuf_write(hpcio_outbuf_t *outbuf, const void *data, size_t size)
{
  size_t amt, amt_done;

  if (outbuf == NULL || outbuf->magic != HPCIO_OUTBUF_MAGIC) {
    return -1;
  }
  if (outbuf->use_lock) {
    spinlock_lock(&outbuf->lock);
  }

  amt_done = 0;
  while (amt_done < size) {
    // flush if needed
    if (size > outbuf->buf_size - outbuf->in_use) {
      outbuf_flush_buffer(outbuf);
      if (outbuf->in_use == outbuf->buf_size) {
	// flush failed, no space
	break;
      }
    }

    // copy as much as possible
    amt = MIN(size - amt_done, outbuf->buf_size - outbuf->in_use);
    memcpy(outbuf->buf_start + outbuf->in_use, data + amt_done, amt);
    outbuf->in_use += amt;
    amt_done += amt;
  }

  if (outbuf->use_lock) {
    spinlock_unlock(&outbuf->lock);
  }
  return amt_done;
}


// Flush the outbuf to the kernel via write().
//
// Returns: HPCFMT_OK on success, else HPCFMT_ERR.
//
int
hpcio_outbuf_flush(hpcio_outbuf_t *outbuf)
{
  if (outbuf == NULL || outbuf->magic != HPCIO_OUTBUF_MAGIC) {
    return HPCFMT_ERR;
  }
  if (outbuf->use_lock) {
    spinlock_lock(&outbuf->lock);
  }

  int ret = outbuf_flush_buffer(outbuf);

  if (outbuf->use_lock) {
    spinlock_unlock(&outbuf->lock);
  }
  return ret;
}


// Flush the outbuf and close() the file descriptor.  Note: the client
// must explicitly call close at the end of the process.  There is no
// auto close.
//
// Returns: HPCFMT_OK on success, else HPCFMT_ERR.
//
int
hpcio_outbuf_close(hpcio_outbuf_t *outbuf)
{
  int ret = HPCFMT_OK;

  if (outbuf == NULL || outbuf->magic != HPCIO_OUTBUF_MAGIC) {
    return HPCFMT_ERR;
  }
  if (outbuf->use_lock) {
    spinlock_lock(&outbuf->lock);
  }

  if (outbuf_flush_buffer(outbuf) == HPCFMT_OK
      && close(outbuf->fd) == 0) {
    // flush and close both succeed
    outbuf->magic = 0;
    outbuf->fd = -1;
  }
  else {
    ret = HPCFMT_ERR;
  }

  if (outbuf->use_lock) {
    spinlock_unlock(&outbuf->lock);
  }
  return ret;
}
