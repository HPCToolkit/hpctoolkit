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
// Copyright ((c)) 2002-2018, Rice University
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

//******************************************************************************
// File: threadmgr.c: 
// Purpose: maintain information about the number of live threads
//******************************************************************************



//******************************************************************************
// system include files 
//******************************************************************************
#include <stdint.h>



//******************************************************************************
// local include files 
//******************************************************************************
#include "threadmgr.h"
#include <lib/prof-lean/stdatomic.h>


//******************************************************************************
// private data
//******************************************************************************
static atomic_int_least32_t threadmgr_active_threads = ATOMIC_VAR_INIT(1); // one for the process main thread



//******************************************************************************
// private operations
//******************************************************************************

static void
adjust_thread_count(int32_t val)
{
	atomic_fetch_add_explicit(&threadmgr_active_threads, val, memory_order_relaxed);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_threadmgr_thread_new()
{
	adjust_thread_count(1);
}


void 
hpcrun_threadmgr_thread_delete()
{
	adjust_thread_count(-1);
}


int 
hpcrun_threadmgr_thread_count()
{
	return atomic_load_explicit(&threadmgr_active_threads, memory_order_relaxed);
}
