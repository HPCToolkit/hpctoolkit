// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/trunk/src/tool/hpcrun/unwind/x86-family/manual-intervals/x86-intel11-f90main.c $
// $Id: x86-intel11-f90main.c 4422 2014-02-10 21:24:59Z mwkrentel $
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

#include <string.h>
#include "x86-unwind-interval-fixup.h"
#include "x86-unwind-interval.h"

static char gcc_main64_signature[] = { 
 0x4c, 0x8d, 0x54, 0x24, 0x08,  // lea    0x8(%rsp),%r10
 0x48, 0x83, 0xe4, 0xe0,        // and    $0xffffffffffffffe0,%rsp
 0x41, 0xff, 0x72, 0xf8,        // pushq  -0x8(%r10)
 0x55,                          // push   %rbp
 0x48, 0x89, 0xe5,              // mov    %rsp,%rbp
};

static int 
adjust_gcc_main64_intervals(char *ins, int len, interval_status *stat)
{
  int siglen = sizeof(gcc_main64_signature);

  if (len > siglen && strncmp((char *)gcc_main64_signature, ins, siglen) == 0) {
    // signature matched 
    unwind_interval *ui = (unwind_interval *) stat->first;

    // this won't fix all of the intervals, but it will fix the ones 
    // that we care about.
    //
    // The method is as follows:
    // Ignore (do not correct) intervals before 1st std frame
    // For 1st STD_FRAME, compute the corrections for this interval and subsequent intervals
    // For this interval and subsequent interval, apply the corrected offsets
    //

    for(; ui->ra_status != RA_STD_FRAME; ui = (unwind_interval *)(ui->common).next);

    // this is only correct for 64-bit code
    for(; ui; ui = (unwind_interval *)(ui->common).next) {
      if (ui->ra_status == RA_SP_RELATIVE) continue;
      if ((ui->ra_status == RA_STD_FRAME) || (ui->ra_status == RA_BP_FRAME)) {  
         ui->ra_status = RA_BP_FRAME;
         ui->bp_ra_pos = 8;
         ui->bp_bp_pos = 0;
      }
    }

    return 1;
  } 
  return 0;
}


static void 
__attribute__ ((constructor))
register_unwind_interval_fixup_function(void)
{
  add_x86_unwind_interval_fixup_function(adjust_gcc_main64_intervals);
}


