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
//   LUSH Interface: Callback Interface for LUSH agents
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_lush_cb_h
#define lush_lush_cb_h

//************************* System Include Files ****************************

#include <stdlib.h>

//*************************** User Include Files ****************************

#include "lush-support-rt.h"

//*************************** Forward Declarations **************************

// **************************************************************************
// A LUSH agent expects the following callbacks:
// **************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------
// Interface for 'heap memory' allocation
// ---------------------------------------------------------

LUSHI_DECL(void*, LUSHCB_malloc, (size_t size));
LUSHI_DECL(void,  LUSHCB_free, ());

// ---------------------------------------------------------
// Facility for unwinding physical stack
// ---------------------------------------------------------

typedef hpcrun_unw_cursor_t LUSHCB_cursor_t;

// LUSHCB_step: Given a cursor, step the cursor to the next (less
// deeply nested) frame.  Conforms to the semantics of libunwind's
// unw_step.  In particular, returns:
//   > 0 : successfully advanced cursor to next frame
//     0 : previous frame was the end of the unwind
//   < 0 : error condition
LUSHI_DECL(int, LUSHCB_step, (LUSHCB_cursor_t* cursor));


LUSHI_DECL(int, LUSHCB_loadmap_find, (void* addr, 
				      char *module_name,
				      void** start, 
				      void** end));

#ifdef __cplusplus
}
#endif

// **************************************************************************

#endif /* lush_lush_cb_h */
