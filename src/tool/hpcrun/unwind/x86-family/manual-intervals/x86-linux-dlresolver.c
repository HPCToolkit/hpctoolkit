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

#include <string.h>
#include "x86-unwind-interval-fixup.h"
#include "x86-unwind-interval.h"

// code snippets from ld-2.17.so

static char dl_runtime_resolve_signature_1[] = { 
   0x48, 0x83, 0xec, 0x78,                   // sub    $0x78,%rsp
   0x48, 0x89, 0x44, 0x24, 0x40,             // mov    %rax,0x40(%rsp)
   0x48, 0x89, 0x4c, 0x24, 0x48,             // mov    %rcx,0x48(%rsp)
   0x48, 0x89, 0x54, 0x24, 0x50,             // mov    %rdx,0x50(%rsp)
   0x48, 0x89, 0x74, 0x24, 0x58,             // mov    %rsi,0x58(%rsp)
   0x48, 0x89, 0x7c, 0x24, 0x60,             // mov    %rdi,0x60(%rsp)
   0x4c, 0x89, 0x44, 0x24, 0x68,             // mov    %r8,0x68(%rsp)
   0x4c, 0x89, 0x4c, 0x24, 0x70,             // mov    %r9,0x70(%rsp)
   0x66, 0x0f, 0x1b, 0x04, 0x24,             // bndmov %bnd0,(%rsp)
   0x66, 0x0f, 0x1b, 0x4c, 0x24, 0x10,       // bndmov %bnd1,0x10(%rsp)
   0x66, 0x0f, 0x1b, 0x54, 0x24, 0x20,       // bndmov %bnd2,0x20(%rsp)
   0x66, 0x0f, 0x1b, 0x5c, 0x24, 0x30,       // bndmov %bnd3,0x30(%rsp)
   0x48, 0x8b, 0xb4, 0x24, 0x80, 0x00, 0x00  // mov    0x80(%rsp),%rsi
};


static char dl_runtime_resolve_signature_2[] = { 
 0x48, 0x83, 0xec, 0x38,        // sub    $0x38,%rsp
 0x48, 0x89, 0x04, 0x24,        // mov    %rax,(%rsp)
 0x48, 0x89, 0x4c, 0x24, 0x08,  // mov    %rcx,0x8(%rsp)
 0x48, 0x89, 0x54, 0x24, 0x10,  // mov    %rdx,0x10(%rsp)
 0x48, 0x89, 0x74, 0x24, 0x18,  // mov    %rsi,0x18(%rsp)
 0x48, 0x89, 0x7c, 0x24, 0x20,  // mov    %rdi,0x20(%rsp)
 0x4c, 0x89, 0x44, 0x24, 0x28,  // mov    %r8,0x28(%rsp)
 0x4c, 0x89, 0x4c, 0x24, 0x30,  // mov    %r9,0x30(%rsp)
 0x48, 0x8b, 0x74, 0x24, 0x40   // mov    0x40(%rsp),%rsi
};


static int matches(char *ins, int len, const char *sig)
{
  int siglen = sizeof(sig);
  return (len > siglen && strncmp((char *)sig, ins, siglen) == 0);
}


static int 
adjust_dl_runtime_resolve_unwind_intervals(char *ins, int len, btuwi_status_t *stat)
{

  if (matches(ins, len, dl_runtime_resolve_signature_1) ||
      matches(ins, len, dl_runtime_resolve_signature_2)) {
	// one of the signatures matched
	unwind_interval *ui = stat->first;
	while(ui) {
	  UWI_RECIPE(ui)->reg.sp_ra_pos += 16;
	  ui = UWI_NEXT(ui);
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


