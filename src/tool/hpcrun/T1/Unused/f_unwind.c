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
