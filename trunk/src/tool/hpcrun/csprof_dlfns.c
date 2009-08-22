// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
//   [...]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   [...]
//
//***************************************************************************

#include "csprof_dlfns.h"
#include "fnbounds_interface.h"
#include "sample_event.h"

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/spinlock.h>


//
// Protect dlopen(), dlclose() and dl_iterate_phdr() with a readers-
// writers lock.  That is, either one writer and no readers, or else
// no writers and any number of readers.
//
// dlopen and dlclose modify the internal dl data structures, so
// they're writers, dl_iterate_phdr (via sampling or pthread_create)
// is a reader.  Note: this lock needs to be process-wide.
//
// And if we could just separate dlopen() into mmap() and its init
// constructor, then we'd only need to block the mmap part. :-(
//
// DLOPEN_RISKY disables the read locks (always succeed), so that
// sampling will never be blocked in this case.  But we keep the write
// locks for the benefit of the fnbounds functions.
//
static spinlock_t dlopen_lock = SPINLOCK_UNLOCKED;
static volatile long dlopen_num_readers = 0;
static volatile char dlopen_num_writers = 0;
static long num_dlopen_pending = 0;


// We use this only in the DLOPEN_RISKY case.
long
csprof_dlopen_pending(void)
{
  return num_dlopen_pending;
}


// Writers always wait until they acquire the lock.
static void
csprof_dlopen_write_lock(void)
{
  int acquire = 0;

  do {
    spinlock_lock(&dlopen_lock);
    if (dlopen_num_writers == 0) {
      dlopen_num_writers = 1;
      acquire = 1;
    }
    spinlock_unlock(&dlopen_lock);
  } while (! acquire);

  // Wait for any readers to finish.
  if (! ENABLED(DLOPEN_RISKY)) {
    while (dlopen_num_readers > 0) ;
  }
}


static void
csprof_dlopen_write_unlock(void)
{
  dlopen_num_writers = 0;
}


// Downgrade the dlopen lock from a writers lock to a readers lock.
// Must already hold the writers lock.
static void
csprof_dlopen_downgrade_lock(void)
{
  atomic_add_i64(&dlopen_num_readers, 1L);
  dlopen_num_writers = 0;
}


// Readers try to acquire a lock, but they don't wait if that fails.
// Returns: 1 if acquired, else 0 if not.
int
csprof_dlopen_read_lock(void)
{
  int acquire = 0;

  spinlock_lock(&dlopen_lock);
  if (dlopen_num_writers == 0 || ENABLED(DLOPEN_RISKY)) {
    atomic_add_i64(&dlopen_num_readers, 1L);
    acquire = 1;
  }
  spinlock_unlock(&dlopen_lock);

  return (acquire);
}


void
csprof_dlopen_read_unlock(void)
{
  atomic_add_i64(&dlopen_num_readers, -1L);
}


void 
csprof_pre_dlopen(const char *path, int flags)
{
  csprof_dlopen_write_lock();
  atomic_add_i64(&num_dlopen_pending, 1L);
}


// It's safe to downgrade the lock during fnbounds_map_open_dsos()
// because it acquires the dl-iterate lock before the fnbounds lock,
// and that order is consistent with sampling.
//
void 
csprof_dlopen(const char *module_name, int flags, void *handle)
{
  TMSG(EPOCH, "dlopen: handle = %p, name = %s", handle, module_name);
  csprof_dlopen_downgrade_lock();
  fnbounds_map_open_dsos();
  atomic_add_i64(&num_dlopen_pending, -1L);
  csprof_dlopen_read_unlock();
}


void
csprof_dlclose(void *handle)
{
  csprof_dlopen_write_lock();
}


// We can't downgrade the lock during fnbounds_unmap_closed_dsos()
// because it acquires the fnbounds lock before the dl-iterate lock,
// and that is a LOR with sampling.
//
void
csprof_post_dlclose(void *handle, int ret)
{
  TMSG(EPOCH, "dlclose: handle = %p", handle);
  fnbounds_unmap_closed_dsos();
  csprof_dlopen_write_unlock();
}
