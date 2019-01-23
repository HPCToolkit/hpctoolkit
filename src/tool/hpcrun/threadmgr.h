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
// Copyright ((c)) 2002-2019, Rice University
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
// File: threadmgr.h 
//
// Purpose: 
//   interface definitions for threadmgr, which maintains information 
//   about the number of live threads
//******************************************************************************

#ifndef _threadmgr_h_
#define _threadmgr_h_

#include "thread_data.h"


//******************************************************************************
// constants
//******************************************************************************

#define OPTION_NO_COMPACT_THREAD  0
#define OPTION_COMPACT_THREAD     1

//******************************************************************************
// interface operations
//******************************************************************************

void hpcrun_threadmgr_thread_new();

void hpcrun_threadmgr_thread_delete();

int hpcrun_threadmgr_thread_count();

bool
hpcrun_threadMgr_data_get(int id, cct_ctxt_t* thr_ctxt, thread_data_t **data);

void
hpcrun_threadMgr_data_put( epoch_t *epoch, thread_data_t *data );

void
hpcrun_threadMgr_data_fini(thread_data_t *td);

int
hpcrun_threadMgr_compact_thread();

#endif
