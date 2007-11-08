#include "prim_unw.h"
#include "general.h"

#define UNW_REG_IP 1

extern void *monitor_unwind_fence1,*monitor_unwind_fence2;

int csprof_check_fence(void *ip){
  return (ip >= &monitor_unwind_fence1) && (ip <= &monitor_unwind_fence2);
}

void unw_init_f_context(void *context, unw_cursor_t *cursor);

void t_sample_callstack(void *context){

  void *ip;
  int first = 1;
  unw_cursor_t frame,*cursor;

  return;

  cursor = &frame;
  unw_init_f_context(context,&frame);

  for(;;){
    if (unw_get_reg (cursor, UNW_REG_IP, &ip) < 0){
      MSG(1,"get_reg break");
      break;
    }
    if (first){
      MSG(1,"Starting IP = %p",(void *)ip);
    }
    if ((unw_step (cursor) <= 0) || csprof_check_fence(ip)){
      MSG(1,"Hit unw_step break");
      break;
    }
  }
}
