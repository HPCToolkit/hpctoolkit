#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "csprof-malloc.h"
#include "intervals.h"
#include "pmsg.h"
#include "atomic-ops.h"

#define STR(s) case s: return #s

/*************************************************************************************
 * forward declarations
 ************************************************************************************/

static const char *ra_status_string(ra_loc l); 
static const char *bp_status_string(bp_loc l);



/*************************************************************************************
 * global variables
 ************************************************************************************/

const unwind_interval poison_ui = {
  0L,
  0L,
  POISON,
  0,
  0,
  BP_HOSED,
  0,
  0,
  NULL,
  NULL
};



long ui_cnt = 0;
long suspicious_cnt = 0;


/*************************************************************************************
 * interface operations 
 ************************************************************************************/

long 
ui_count() 
{
  return ui_cnt;
}

long 
suspicious_count() 
{
  return suspicious_cnt;
}


void
suspicious_interval(void *pc) 
{
  EMSG("suspicous interval for pc = %p", pc);
  fetch_and_add(&suspicious_cnt,1);
}

unwind_interval *
new_ui(char *startaddr, ra_loc ra_status, unsigned int sp_ra_pos,
       int bp_ra_pos, bp_loc bp_status, int sp_bp_pos, int bp_bp_pos,
       unwind_interval *prev)
{
  unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval)); 

  (void) fetch_and_add(&ui_cnt, 1);

  u->startaddr = startaddr;
  u->endaddr = 0;

  u->ra_status = ra_status;
  u->sp_ra_pos = sp_ra_pos;
  u->sp_bp_pos = sp_bp_pos;

  u->bp_status = bp_status;
  u->bp_bp_pos = bp_bp_pos;
  u->bp_ra_pos = bp_ra_pos;

  u->prev = prev;
  u->next = NULL;
  u->prev_canonical = NULL;
  u->restored_canonical = 0;

  return u; 
}


void
set_ui_canonical(unwind_interval *u, unwind_interval *value)
{
  u->prev_canonical = value;
} 

void
set_ui_restored_canonical(unwind_interval *u, unwind_interval *value)
{
  u->restored_canonical = 1;
  u->prev_canonical = value;
} 


unwind_interval *
fluke_ui(char *loc,unsigned int pos)
{
  unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval)); 

  u->startaddr = loc;
  u->endaddr = loc;

  u->ra_status = RA_SP_RELATIVE;
  u->sp_ra_pos = pos;

  u->next = NULL;
  u->prev = NULL;

  return u; 
}

void 
link_ui(unwind_interval *current, unwind_interval *next)
{
  current->endaddr = next->startaddr;
  current->next= next;
}


void 
dump_ui(unwind_interval *u, int dump_to_stdout)
{
  char buf[1000];

  sprintf(buf, "start=%p end =%p ra_status=%s sp_ra_pos=%d sp_bp_pos=%d bp_status=%s "
	  "bp_ra_pos = %d bp_bp_pos=%d next=%p prev=%p prev_canonical=%p rest_canon=%d\n", 
	  (void *) u->startaddr, (void *) u->endaddr, ra_status_string(u->ra_status),
	  u->sp_ra_pos, u->sp_bp_pos, 
	  bp_status_string(u->bp_status),
	  u->bp_ra_pos, u->bp_bp_pos,
	  u->next, u->prev, u->prev_canonical, u->restored_canonical); 

#if 0
  PMSG(INTV,buf);
#endif
  EMSG(buf);
  if (dump_to_stdout) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
}



/*************************************************************************************
 * private operations 
 ************************************************************************************/

static const char *
ra_status_string(ra_loc l) 
{
  switch(l) {
   STR(RA_SP_RELATIVE);
   STR(RA_STD_FRAME);
   STR(RA_BP_FRAME);
   STR(RA_REGISTER);
   STR(POISON);
  default:
    assert(0);
  }
  return NULL;
}

static const char *
bp_status_string(bp_loc l)
{
  switch(l){
    STR(BP_UNCHANGED);
    STR(BP_SAVED);
    STR(BP_HOSED);
  default:
    assert(0);
  }
}

