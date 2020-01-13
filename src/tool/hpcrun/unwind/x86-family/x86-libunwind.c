#include <stdio.h>
#include <string.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <messages/messages.h>

#define MYMSG(f,...) hpcrun_pmsg(#f, __VA_ARGS__)

static void 
cursor_set(unw_context_t *uc, unw_cursor_t *cursor, 
           void **sp, void **bp, void *ip) 
{
  // initialize unwind context
  memset(uc, 0, sizeof(*uc));

  // set context registers needed for unwinding
  uc->uc_mcontext.gregs[REG_RBP] = (long) bp;
  uc->uc_mcontext.gregs[REG_RSP] = (long) sp;
  uc->uc_mcontext.gregs[REG_RIP] = (long) ip;

  unw_init_local(cursor, uc);
}

static void
cursor_get(unw_context_t *uc, unw_cursor_t *cursor, 
           void ***sp, void ***bp, void **ip) 
{
  unw_word_t uip, usp, ubp;

  unw_get_reg(cursor, UNW_TDEP_IP, &uip);
  unw_get_reg(cursor, UNW_TDEP_SP, &usp);
  unw_get_reg(cursor, UNW_TDEP_BP, &ubp);

  *sp = (void **) usp;
  *bp = (void **) ubp;
  *ip = (void *) uip;
}


int
libunwind_step(void ***sp, void ***bp, void **ip)
{
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


void show_backtrace (void) {
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_TDEP_IP, &ip);
    unw_get_reg(&cursor, UNW_TDEP_SP, &sp);
    MYMSG(LIBUNWIND, "ip = %lx, sp = %lx\n", (long) ip, (long) sp);
  }
}
