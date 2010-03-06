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
// Copyright ((c)) 2002-2010, Rice University 
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

/* call libunwind f fortran */

#include <stdio.h>
#include <libunwind.h>
#include "msg.h"

extern void tfun_(void){
  MSG("c function called from pgi f90!!\n");
}

extern void init_unw(unw_context_t *ctx,
                     unw_cursor_t *cursor){

  if (unw_getcontext(ctx) < 0){
    MSG("cannot get context\n");
  }
  if(unw_init_local(cursor, ctx) < 0) {
    MSG("Could not initialize unwind cursor!\n");
  }
}

extern void f_unwind_(void){

  unw_context_t ctx;
  unw_cursor_t cursor;
  
  unw_word_t ip;

  MSG("f_unwind start\n");

  if (unw_getcontext(&ctx) < 0){
    MSG("cannot get context\n");
  }
  if(unw_init_local(&cursor, &ctx) < 0) {
    MSG("Could not initialize unwind cursor!\n");
  }

  while(unw_step(&cursor) > 0){
    unw_get_reg(&cursor,UNW_REG_IP,&ip);
    MSG("UNWIND ip = %lx\n",(long)ip);
  }
}

extern void unwind_n(int n){
  unw_context_t ctx;
  unw_cursor_t cursor;
  
  unw_word_t ip;
  if (! n) return;

  init_unw(&ctx,&cursor);
  unw_get_reg(&cursor,UNW_REG_IP,&ip);
  MSG("initial UNWIND ip = %lx\n",(long)ip);
  n--;

  while(n--){
    if (unw_step(&cursor) > 0){
      unw_get_reg(&cursor,UNW_REG_IP,&ip);
      MSG("%d UNWIND ip = %lx\n",n,(long)ip);
    }
  }
}
