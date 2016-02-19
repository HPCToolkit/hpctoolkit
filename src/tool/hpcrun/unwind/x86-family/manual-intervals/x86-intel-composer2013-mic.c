// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/unwind/x86-family/manual-intervals/x86-intel11-f90main.c $
// $Id: x86-intel11-f90main.c 3680 2012-02-25 22:14:00Z krentel $
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

static char intelmic_comp13_for_main_signature[] = { 
  0x53,                         // push   %rbx
  0x48, 0x89, 0xe3,             // mov    %rsp,%rbx
  0x48, 0x83, 0xe4, 0x80,       // and    $0xffffffffffffff80,%rsp
  0x48, 0x83, 0xec, 0x78,       // sub    $0x78,%rsp
  0x55,                         // push   %rbp
  0x48, 0x8b, 0x6b, 0x08,       // mov    0x8(%rbx),%rbp
  0x48, 0x89, 0x6c, 0x24, 0x08, // mov    %rbp,0x8(%rsp)
  0x48, 0x89, 0xe5,             // mov    %rsp,%rbp
};


static char intelmic_comp13_kmp_alloc_thread_signature[] = { 
  0x53,                         // push   %rbx
  0x48, 0x89, 0xe3,             // mov    %rsp,%rbx
  0x48, 0x83, 0xe4, 0xc0,       // and    $0xffffffffffffffc0,%rsp
  0x48, 0x83, 0xec, 0x38,       // sub    $0x38,%rsp
  0x55,                         // push   %rbp
  0x48, 0x8b, 0x6b, 0x08,       // mov    0x8(%rbx),%rbp
  0x48, 0x89, 0x6c, 0x24, 0x08, // mov    %rbp,0x8(%rsp)
  0x48, 0x89, 0xe5,             // mov    %rsp,%rbp
};


static int 
adjust_intelmic_intervals(char *ins, int len, interval_status *stat)
{
  // NOTE: the two signatures above are the same length. The next three lines of code below depend upon that.
  int siglen = sizeof(intelmic_comp13_for_main_signature); 
  if (len > siglen && ((strncmp((char *)intelmic_comp13_for_main_signature, ins, siglen) == 0) || 
		       (strncmp((char *)intelmic_comp13_kmp_alloc_thread_signature, ins, siglen) == 0))) {
    // signature matched 
    unwind_interval *ui = (unwind_interval *) stat->first;
    
    // this won't fix all of the intervals, but it will fix the one we care about.
    while(ui) {
      if (ui->ra_status == RA_STD_FRAME){
	ui->bp_ra_pos = 8;
	ui->bp_bp_pos = 0;
      }
      ui = (unwind_interval *)(ui->common).next;
    }
    
    return 1;
  } 
  return 0;
}


static void 
__attribute__ ((constructor))
register_unwind_interval_fixup_function(void)
{
  add_x86_unwind_interval_fixup_function(adjust_intelmic_intervals);
}
