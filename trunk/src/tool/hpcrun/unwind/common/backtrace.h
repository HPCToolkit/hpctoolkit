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

#ifndef hpcrun_backtrace_h
#define hpcrun_backtrace_h

//***************************************************************************
// file: backtrace.h
//
// purpose:
//     an interface for performing stack unwinding 
//
//***************************************************************************


//***************************************************************************
// system include files 
//***************************************************************************

#include <stddef.h>
#include <stdint.h>

#include <ucontext.h>

//***************************************************************************
// local include files 
//***************************************************************************

#include "state.h"



//***************************************************************************
// forward declarations
//***************************************************************************

csprof_cct_node_t*
hpcrun_backtrace(state_t *state, ucontext_t* context, 
		 int metricId, uint64_t metricIncr,
		 int skipInner, int isSync);

hpcrun_frame_t*
hpcrun_skip_chords(hpcrun_frame_t* bt_outer, hpcrun_frame_t* bt_inner, 
		   int skip);

// FIXME: tallent: relocate when 'csprof state' trash is untangled
void 
dump_backtrace(state_t *state, hpcrun_frame_t *unwind);

//***************************************************************************

#endif // hpcrun_backtrace_h
