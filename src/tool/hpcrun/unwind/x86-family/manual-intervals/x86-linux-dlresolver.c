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

#include <string.h>
#include "x86-unwind-interval-fixup.h"
#include "x86-unwind-interval.h"

static char dl_runtime_resolve_signature[] = { 

 0x48, 0x83, 0xec, 0x38,        // sub    $0x38,%rsp
 0x48, 0x89, 0x04, 0x24,        // mov    %rax,(%rsp)
 0x48, 0x89, 0x4c, 0x24, 0x08,  // mov    %rcx,0x8(%rsp)
 0x48, 0x89, 0x54, 0x24, 0x10,  // mov    %rdx,0x10(%rsp)
 0x48, 0x89, 0x74, 0x24, 0x18,  // mov    %rsi,0x18(%rsp)
 0x48, 0x89, 0x7c, 0x24, 0x20,  // mov    %rdi,0x20(%rsp)
 0x4c, 0x89, 0x44, 0x24, 0x28,  // mov    %r8,0x28(%rsp)
 0x4c, 0x89, 0x4c, 0x24, 0x30,  // mov    %r9,0x30(%rsp)
 0x48, 0x8b, 0x74, 0x24, 0x40,  // mov    0x40(%rsp),%rsi
 0x49, 0x89, 0xf3,              // mov    %rsi,%r11
 0x4c, 0x01, 0xde,              // add    %r11,%rsi
 0x4c, 0x01, 0xde,              // add    %r11,%rsi
 0x48, 0xc1, 0xe6, 0x03,        // shl    $0x3,%rsi
 0x48,                          // rex64
 0x8b,                          // .byte 0x8b
 0x7c, 0x24,                    // jl     60 <HPCTRACE_FMT_Magic>
};

static int 
adjust_dl_runtime_resolve_unwind_intervals(char *ins, int len, interval_status *stat)
{
  int siglen = sizeof(dl_runtime_resolve_signature);

  if (len > siglen && strncmp((char *)dl_runtime_resolve_signature, ins, siglen) == 0) {
    // signature matched 
    unwind_interval *ui = (unwind_interval *) stat->first;
    while(ui) {
	ui->sp_ra_pos += 16;
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
  add_x86_unwind_interval_fixup_function(adjust_dl_runtime_resolve_unwind_intervals);
}


