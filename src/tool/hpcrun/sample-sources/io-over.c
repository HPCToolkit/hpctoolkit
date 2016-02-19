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
// $HeadURL$
//
// Purpose:
// This file adds the IO sampling source: number of bytes read and
// written.  This covers both stream IO (fread, fwrite, etc) and
// unbuffered IO (read, write, etc).
//
// Note: for the slow or blocking overrides, we record samples before
// and after the function.  If a process blocks in kernel, then it
// won't receive async interrupts and this may under report the time
// in the trace.  Using two samples assures that we see the full span
// of the function in the trace viewer.
//
// TODO list:
//
// 2. In the second sample (after the real function), want to record
// the sample without doing a full unwind (which is the same path as
// the first sample).  This may require a little refactoring in the
// sample event code.
//
// 3. When taking the user context, replace the syscall with the
// assembler macros.  This may require a little refactoring of the
// macros so that get context is just one line, not a #ifdef sequence.
// Also, need to fix or disable the i386 case.
//
// 4. Figure out the automake way to strip debug symbols from a static
// archive.  Currently, this only works in the dynamic case.
//
// 5. Add overrides for printf, fprintf, fputs, etc.
//
//***************************************************************************

/******************************************************************************
 * standard include files
 *****************************************************************************/

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>


/******************************************************************************
 * local include files
 *****************************************************************************/

#include <main.h>
#include <safe-sampling.h>
#include <sample_event.h>
#include <thread_data.h>

#include <messages/messages.h>
#include <monitor-exts/monitor_ext.h>
#include <sample-sources/io.h>


/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef ssize_t read_fn_t(int, void *, size_t);
typedef ssize_t write_fn_t(int, const void *, size_t);

typedef size_t  fread_fn_t(void *, size_t, size_t, FILE *);
typedef size_t  fwrite_fn_t(const void *, size_t, size_t, FILE *);


/******************************************************************************
 * macros
 *****************************************************************************/

// Dynamically, there are two ways to get the real version of library
// functions: the __libc_/_IO_ names or dlsym().  The __libc_ names
// are GNU libc specific and may not be portable, and dlsym() may
// interfere with our code via locks or override functions.  We'll try
// the _IO_ names until we hit a problem.  Statically, we always use
// __wrap and __real.

#ifdef HPCRUN_STATIC_LINK
#define real_read    __real_read
#define real_write   __real_write
#define real_fread   __real_fread
#define real_fwrite  __real_fwrite
#else
#define real_read    __read
#define real_write   __write
#define real_fread   _IO_fread
#define real_fwrite  _IO_fwrite
#endif

extern read_fn_t    real_read;
extern write_fn_t   real_write;
extern fread_fn_t   real_fread;
extern fwrite_fn_t  real_fwrite;


/******************************************************************************
 * interface operations
 *****************************************************************************/

ssize_t
MONITOR_EXT_WRAP_NAME(read)(int fd, void *buf, size_t count)
{
  ucontext_t uc;
  ssize_t ret;
  int metric_id_read = hpcrun_metric_id_read();
  int save_errno;

  if (metric_id_read < 0 || ! hpcrun_safe_enter()) {
    return real_read(fd, buf, count);
  }

  // insert samples before and after the slow functions to make the
  // traces look better.
  getcontext(&uc);
  hpcrun_sample_callpath(&uc, metric_id_read, 0, 0, 1);

  hpcrun_safe_exit();
  ret = real_read(fd, buf, count);
  save_errno = errno;
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "read: fd: %d, buf: %p, count: %ld, actual: %ld",
       fd, buf, count, ret);
  hpcrun_sample_callpath(&uc, metric_id_read, (ret > 0 ? ret : 0), 0, 1);
  hpcrun_safe_exit();

  errno = save_errno;
  return ret;
}


ssize_t
MONITOR_EXT_WRAP_NAME(write)(int fd, const void *buf, size_t count)
{
  ucontext_t uc;
  size_t ret;
  int metric_id_write = hpcrun_metric_id_write();
  int save_errno;

  if (metric_id_write < 0 || ! hpcrun_safe_enter()) {
    return real_write(fd, buf, count);
  }

  // insert samples before and after the slow functions to make the
  // traces look better.
  getcontext(&uc);
  hpcrun_sample_callpath(&uc, metric_id_write, 0, 0, 1);

  hpcrun_safe_exit();
  ret = real_write(fd, buf, count);
  save_errno = errno;
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "write: fd: %d, buf: %p, count: %ld, actual: %ld",
       fd, buf, count, ret);
  hpcrun_sample_callpath(&uc, metric_id_write, (ret > 0 ? ret : 0), 0, 1);
  hpcrun_safe_exit();

  errno = save_errno;
  return ret;
}


size_t
MONITOR_EXT_WRAP_NAME(fread)(void *ptr, size_t size, size_t count, FILE *stream)
{
  ucontext_t uc;
  size_t ret;
  int metric_id_read = hpcrun_metric_id_read();

  if (metric_id_read < 0 || ! hpcrun_safe_enter()) {
    return real_fread(ptr, size, count, stream);
  }

  // insert samples before and after the slow functions to make the
  // traces look better.

  getcontext(&uc);
  hpcrun_sample_callpath(&uc, metric_id_read, 0, 0, 1);

  hpcrun_safe_exit();
  ret = real_fread(ptr, size, count, stream);
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "fread: size: %ld, count: %ld, bytes: %ld, actual: %ld",
       size, count, count*size, ret*size);
  hpcrun_sample_callpath(&uc, metric_id_read, ret*size, 0, 1);
  hpcrun_safe_exit();

  return ret;
}


size_t
MONITOR_EXT_WRAP_NAME(fwrite)(const void *ptr, size_t size, size_t count,
			      FILE *stream)
{
  ucontext_t uc;
  size_t ret;
  int metric_id_write = hpcrun_metric_id_write();

  if (metric_id_write < 0 || ! hpcrun_safe_enter()) {
    return real_fwrite(ptr, size, count, stream);
  }

  // insert samples before and after the slow functions to make the
  // traces look better.
  getcontext(&uc);
  hpcrun_sample_callpath(&uc, metric_id_write, 0, 0, 1);

  hpcrun_safe_exit();
  ret = real_fwrite(ptr, size, count, stream);
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "fwrite: size: %ld, count: %ld, bytes: %ld, actual: %ld",
       size, count, count*size, ret*size);
  hpcrun_sample_callpath(&uc, metric_id_write, ret*size, 0, 1);
  hpcrun_safe_exit();

  return ret;
}
