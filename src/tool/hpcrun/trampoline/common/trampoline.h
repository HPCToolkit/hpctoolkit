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

#ifndef trampoline_h
#define trampoline_h

//******************************************************************************
// File: trampoline.h
//
// Purpose: architecture independent support for counting returns of sampled
//          frames using trampolines
//
// Modification History:
//   2009/09/15 - created - Mike Fagan and John Mellor-Crummey
//******************************************************************************

// *****************************************************************************
//    System Includes
// *****************************************************************************

#include <stdbool.h>


// *****************************************************************************
//    Local Includes
// *****************************************************************************

#include <hpcrun/thread_data.h>
#include <cct/cct.h>

// *****************************************************************************
//    Constants
// *****************************************************************************

enum trampoline_constants {
  CACHED_BACKTRACE_SIZE = 32
};

// *****************************************************************************
//    Interface Functions
// *****************************************************************************


extern void hpcrun_init_trampoline_info(void);

// returns true if address is in the assembly language trampoline code, else false.

extern bool hpcrun_trampoline_interior(void* addr);
extern bool hpcrun_trampoline_at_entry(void* addr);

extern void* hpcrun_trampoline_handler(void);

extern void hpcrun_trampoline_insert(cct_node_t* node);
extern void hpcrun_trampoline_remove(void);
extern void hpcrun_trampoline_bt_dump(void);

#endif // trampoline_h
