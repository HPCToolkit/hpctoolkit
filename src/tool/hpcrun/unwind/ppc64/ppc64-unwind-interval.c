/*****************************************************************************
 * system include files 
 *****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <assert.h>



/*****************************************************************************
 * local include files 
 *****************************************************************************/

#include "csprof-malloc.h"
#include "ppc64-unwind-interval.h"
#include "pmsg.h"
#include "atomic-ops.h"



/*****************************************************************************
 * macros
 *****************************************************************************/

#define STR(s) case s: return #s



/*****************************************************************************
 * forward declarations
 *****************************************************************************/

static const char *ra_status_string(ra_loc l); 
static const char *bp_status_string(bp_loc l);



/*****************************************************************************
 * global variables
 *****************************************************************************/

const unwind_interval poison_ui = {
  .common    = {
    .next  = NULL,
    .prev  = NULL
  },

  .ra_status = POISON,

  .bp_status = BP_HOSED,
  .bp_ra_pos = 0,
};


long ui_cnt = 0;
long suspicious_cnt = 0;


/*****************************************************************************
 * interface operations 
 *****************************************************************************/

long 
ui_count(void)
{
  return ui_cnt;
}


long 
suspicious_count(void)
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
new_ui(char *start, 
       ra_loc ra_status, int bp_ra_pos, 
       bp_loc bp_status,          
       unwind_interval *prev)
{
  unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval)); 

// **** SPECIAL PURPOSE CODE TO INDUCE MEM FAILURE (conditionally included) ***
# include "mem_error_gen.h" 

  (void) fetch_and_add(&ui_cnt, 1);

  u->common.start = start;
  u->common.end = 0;

  u->ra_status = ra_status;
  u->bp_status = bp_status;
  u->bp_ra_pos = bp_ra_pos;

  u->common.prev = (splay_interval_t *) prev;
  u->common.next = NULL;

  return u; 
}


void 
link_ui(unwind_interval *current, unwind_interval *next)
{
  current->common.end = next->common.start;
  current->common.next= (splay_interval_t *)next;
}


void 
dump_ui(unwind_interval *u, int dump_to_stdout)
{
  char buf[1000];

  sprintf(buf, "start=%p end =%p ra_status=%s bp_status=%s "
	  "bp_ra_pos = %d next=%p prev=%p\n", 
	  (void *) u->common.start, (void *) u->common.end, 
	  ra_status_string(u->ra_status), bp_status_string(u->bp_status), 
	  u->bp_ra_pos, u->common.next, u->common.prev); 

  EMSG(buf);
  if (dump_to_stdout) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
}



/*****************************************************************************
 * private operations 
 *****************************************************************************/

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
