// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/unwind/x86-family/manual-intervals/x86-intel11-f90main.c $
// $Id: x86-intel11-f90main.c 3543 2011-05-27 22:26:29Z mfagan $
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

#include <string.h>
#include "x86-unwind-interval-fixup.h"
#include "x86-unwind-interval.h"

static char pgi_mp_pexit_signature[] = {
  0x41,  0x5b,                			// pop    %r11
  0x4c, 0x89, 0x9c, 0x24, 0xb0, 0x00, 0x00, 	// mov    %r11,0xb0(%rsp)
  0x00, 
  0x48, 0x83, 0x7c, 0x24, 0x08, 0x01,    	// cmpq   $0x1,0x8(%rsp)
};


int 
x86_adjust_pgi_mp_pexit_intervals(char *ins, int len, btuwi_status_t *stat)
{
  int siglen = sizeof(pgi_mp_pexit_signature);

  if (len > siglen && strncmp((char *)pgi_mp_pexit_signature, ins, siglen) == 0) {
    // signature matched 
    unwind_interval *ui = stat->first;

    // this won't fix all of the intervals, but it will fix the one we care about.
    while(ui) {
      x86recipe_t *xr = UWI_RECIPE(ui);
      if (xr->ra_status == RA_SP_RELATIVE){
    	xr->reg.sp_ra_pos = 0xb0;
    	xr->reg.sp_bp_pos = 0;
      }
      ui = UWI_NEXT(ui);
    }
    return 1;
  } 
  return 0;
}
