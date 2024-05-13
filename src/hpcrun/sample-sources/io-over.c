// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include "io-over.h"

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

#include "io-over.h"

#include "../main.h"
#include "../safe-sampling.h"
#include "../sample_event.h"
#include "../thread_data.h"

#include "../messages/messages.h"
#include "io.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

ssize_t
foilbase_read(read_fn_t* real_read, int fd, void *buf, size_t count)
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
  hpcrun_sample_callpath(&uc, metric_id_read,
        (hpcrun_metricVal_t) {.i=0},
        0, 1, NULL);

  hpcrun_safe_exit();
  ret = real_read(fd, buf, count);
  save_errno = errno;
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "read: fd: %d, buf: %p, count: %ld, actual: %ld",
       fd, buf, count, ret);
  hpcrun_sample_callpath(&uc, metric_id_read,
        (hpcrun_metricVal_t) {.i=(ret > 0 ? ret : 0)},
        0, 1, NULL);
  hpcrun_safe_exit();

  errno = save_errno;
  return ret;
}


ssize_t
foilbase_write(write_fn_t* real_write, int fd, const void *buf, size_t count)
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
  hpcrun_sample_callpath(&uc, metric_id_write,
            (hpcrun_metricVal_t) {.i=0},
            0, 1, NULL);

  hpcrun_safe_exit();
  ret = real_write(fd, buf, count);
  save_errno = errno;
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "write: fd: %d, buf: %p, count: %ld, actual: %ld",
       fd, buf, count, ret);
  hpcrun_sample_callpath(&uc, metric_id_write,
        (hpcrun_metricVal_t) {.i=(ret > 0 ? ret : 0)},
        0, 1, NULL);
  hpcrun_safe_exit();

  errno = save_errno;
  return ret;
}


size_t
foilbase_fread(fread_fn_t* real_fread, void *ptr, size_t size, size_t count, FILE *stream)
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
  hpcrun_sample_callpath(&uc, metric_id_read,
            (hpcrun_metricVal_t) {.i=0},
            0, 1, NULL);

  hpcrun_safe_exit();
  ret = real_fread(ptr, size, count, stream);
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "fread: size: %ld, count: %ld, bytes: %ld, actual: %ld",
       size, count, count*size, ret*size);
  hpcrun_sample_callpath(&uc, metric_id_read,
            (hpcrun_metricVal_t) {.i=ret*size},
            0, 1, NULL);
  hpcrun_safe_exit();

  return ret;
}


size_t
foilbase_fwrite(fwrite_fn_t* real_fwrite, const void *ptr, size_t size, size_t count, FILE *stream)
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
  hpcrun_sample_callpath(&uc, metric_id_write,
            (hpcrun_metricVal_t) {.i=0},
            0, 1, NULL);

  hpcrun_safe_exit();
  ret = real_fwrite(ptr, size, count, stream);
  hpcrun_safe_enter();

  // FIXME: the second sample should not do a full unwind.
  TMSG(IO, "fwrite: size: %ld, count: %ld, bytes: %ld, actual: %ld",
       size, count, count*size, ret*size);
  hpcrun_sample_callpath(&uc, metric_id_write,
            (hpcrun_metricVal_t) {.i=ret*size},
            0, 1, NULL);
  hpcrun_safe_exit();

  return ret;
}
