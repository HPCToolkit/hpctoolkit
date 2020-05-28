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

#include "hpcrun_dlfns.h"
#include "fnbounds_interface.h"
#include "sample_event.h"
#include "thread_data.h"

#include <messages/messages.h>
#include <lib/prof-lean/spinlock.h>
#include <monitor.h>


//
// Protect dlopen(), dlclose() and dl_iterate_phdr() with a readers-
// writers lock.  That is, either one writer and no readers, or else
// no writers and any number of readers.
//
// dlopen and dlclose modify the internal dl data structures, so
// they're writers, dl_iterate_phdr (via sampling or pthread_create)
// is a reader.  Note: this lock needs to be process-wide.
//
// Now allow a writer to lock against itself.  That is, we record the
// thread id of the writer and if a nested dlopen (from init ctor)
// happens in the same thread, then we allow the thread to proceed.
// Normally, this would be dangerous (exposes inconsistent state).
// But in this case, the init ctor occurs after the dangerous part.
// This is necessary to handle nested dlopens in an init constructor.
//
// And if we could just separate dlopen() into mmap() and its init
// constructor, then we'd only need to block the mmap part. :-(
//
// DLOPEN_RISKY disables the read locks (always succeed), so that
// sampling will never be blocked in this case.  But we keep the write
// locks for the benefit of the fnbounds functions.
//

#if 0
static spinlock_t dlopen_lock = SPINLOCK_UNLOCKED;
static atomic_long dlopen_num_readers = ATOMIC_VAR_INIT(0);
static volatile long dlopen_num_writers = 0;
static int  dlopen_writer_tid = -1;
#endif

static atomic_long num_dlopen_pending = ATOMIC_VAR_INIT(0);


// We use this only in the DLOPEN_RISKY case.
long
hpcrun_dlopen_pending(void)
{
  return atomic_load_explicit(&num_dlopen_pending, memory_order_relaxed);
}

void 
hpcrun_pre_dlopen(const char *path, int flags)
{
    
}


void 
hpcrun_dlopen(const char *module_name, int flags, void *handle)
{
  fnbounds_map_open_dsos();
}


void
hpcrun_dlclose(void *handle)
{

}

void
hpcrun_post_dlclose(void *handle, int ret)
{  
  TMSG(LOADMAP, "dlclose: handle = %p", handle);
  fnbounds_unmap_closed_dsos();
}
