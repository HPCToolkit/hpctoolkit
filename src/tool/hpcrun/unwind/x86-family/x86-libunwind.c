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
// Copyright ((c)) 2002-2022, Rice University
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

#include <stdio.h>
#include <string.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <messages/messages.h>

#define MYMSG(f, ...) hpcrun_pmsg(#f, __VA_ARGS__)

static void cursor_set(unw_context_t* uc, unw_cursor_t* cursor, void** sp, void** bp, void* ip) {
  // initialize unwind context
  memset(uc, 0, sizeof(*uc));

  // set context registers needed for unwinding
  uc->uc_mcontext.gregs[REG_RBP] = (long)bp;
  uc->uc_mcontext.gregs[REG_RSP] = (long)sp;
  uc->uc_mcontext.gregs[REG_RIP] = (long)ip;

  unw_init_local(cursor, uc);
}

static void cursor_get(unw_context_t* uc, unw_cursor_t* cursor, void*** sp, void*** bp, void** ip) {
  unw_word_t uip, usp, ubp;

  unw_get_reg(cursor, UNW_TDEP_IP, &uip);
  unw_get_reg(cursor, UNW_TDEP_SP, &usp);
  unw_get_reg(cursor, UNW_TDEP_BP, &ubp);

  *sp = (void**)usp;
  *bp = (void**)ubp;
  *ip = (void*)uip;
}

int libunwind_step(void*** sp, void*** bp, void** ip) {
  int status;
  unw_context_t uc;
  unw_cursor_t cursor;

  // temporarily splice this out
  return 0;

  cursor_set(&uc, &cursor, *sp, *bp, *ip);
  status = unw_step(&cursor) > 0;
  if (status) {
    cursor_get(&uc, &cursor, sp, bp, ip);
  }
  return status;
}

void show_backtrace(void) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_TDEP_IP, &ip);
    unw_get_reg(&cursor, UNW_TDEP_SP, &sp);
    MYMSG(LIBUNWIND, "ip = %lx, sp = %lx\n", (long)ip, (long)sp);
  }
}
