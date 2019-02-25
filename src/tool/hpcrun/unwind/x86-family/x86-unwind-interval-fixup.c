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

//------------------------------------------------------------------------------
// system includes
//------------------------------------------------------------------------------
#include <stdlib.h>



//------------------------------------------------------------------------------
// local includes
//------------------------------------------------------------------------------

#include "x86-unwind-interval-fixup.h"
#include "manual-intervals/x86-manual-intervals.h"



//------------------------------------------------------------------------------
// macros
//------------------------------------------------------------------------------

#define DECLARE_FIXUP_ROUTINE(fn) \
        extern int fn(char *ins, int len, btuwi_status_t* stat); 

#define REGISTER_FIXUP_ROUTINE(fn) fn,



//------------------------------------------------------------------------------
// external declarations
//------------------------------------------------------------------------------

FORALL_X86_INTERVAL_FIXUP_ROUTINES(DECLARE_FIXUP_ROUTINE)



//------------------------------------------------------------------------------
// local variables
//------------------------------------------------------------------------------

static x86_ui_fixup_fn_t x86_interval_fixup_routine_vector[] = {
  FORALL_X86_INTERVAL_FIXUP_ROUTINES(REGISTER_FIXUP_ROUTINE)
  0
};



//------------------------------------------------------------------------------
// interface operations 
//------------------------------------------------------------------------------

int
x86_fix_unwind_intervals(char *ins, int len, btuwi_status_t *stat)
{
   x86_ui_fixup_fn_t *fn = &x86_interval_fixup_routine_vector[0];

   while (*fn) {
    if ((*fn++)(ins,len,stat)) return 1;
   }
   return 0;
}
