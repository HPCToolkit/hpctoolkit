#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "csprof-malloc.h"
#include "x86-unwind-interval.h"
#include "pmsg.h"
#include "atomic-ops.h"
#include "ui_tree.h"

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
  .common    = {
    .next  = NULL,
    .prev  = NULL
  },

  .ra_status = POISON,
  .sp_ra_pos = 0,
  .sp_bp_pos = 0,

  .bp_status = BP_HOSED,
  .bp_ra_pos = 0,
  .bp_bp_pos = 0,

  .prev_canonical     = NULL,
  .restored_canonical = 0
};



long ui_cnt = 0;
long suspicious_cnt = 0;


/*************************************************************************************
 * interface operations 
 ************************************************************************************/

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
  TMSG(SUSPICIOUS_INTERVAL,"suspicious interval for pc = %p", pc);
  fetch_and_add(&suspicious_cnt,1);
}

unwind_interval *
new_ui(char *start, 
       ra_loc ra_status, unsigned int sp_ra_pos, int bp_ra_pos, 
       bp_loc bp_status,          int sp_bp_pos, int bp_bp_pos,
       unwind_interval *prev)
{
  unwind_interval *u = (unwind_interval *) csprof_ui_malloc(sizeof(unwind_interval)); 

# include "mem_error_gen.h" // **** SPECIAL PURPOSE CODE TO INDUCE MEM FAILURE (conditionally included) ***

  (void) fetch_and_add(&ui_cnt, 1);

  u->common.start = start;
  u->common.end = 0;

  u->ra_status = ra_status;
  u->sp_ra_pos = sp_ra_pos;
  u->sp_bp_pos = sp_bp_pos;

  u->bp_status = bp_status;
  u->bp_bp_pos = bp_bp_pos;
  u->bp_ra_pos = bp_ra_pos;

  u->common.prev = (splay_interval_t *) prev;
  u->common.next = NULL;
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
  unwind_interval *u = (unwind_interval *) csprof_ui_malloc(sizeof(unwind_interval)); 

  u->common.start = loc;
  u->common.end = loc;

  u->ra_status = RA_SP_RELATIVE;
  u->sp_ra_pos = pos;

  u->common.next = NULL;
  u->common.prev = NULL;

  return u; 
}

void 
link_ui(unwind_interval *current, unwind_interval *next)
{
  current->common.end = next->common.start;
  current->common.next= (splay_interval_t *)next;
}

static void
_dump_ui_str(unwind_interval *u, char *buf, size_t len)
{
  snprintf(buf, len, "UNW: start=%p end =%p ra_status=%s sp_ra_pos=%d sp_bp_pos=%d bp_status=%s "
	  "bp_ra_pos = %d bp_bp_pos=%d next=%p prev=%p prev_canonical=%p rest_canon=%d\n", 
	  (void *) u->common.start, (void *) u->common.end, ra_status_string(u->ra_status),
	  u->sp_ra_pos, u->sp_bp_pos, 
	  bp_status_string(u->bp_status),
	  u->bp_ra_pos, u->bp_bp_pos,
	  u->common.next, u->common.prev, u->prev_canonical, u->restored_canonical); 
}


void
dump_ui_log(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  EMSG(buf);
}

void
dump_ui(unwind_interval *u, int dump_to_stderr)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  PMSG(UNW,buf);
  if (dump_to_stderr) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
}

void
dump_ui_stderr(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  EEMSG(buf);
}

void
dump_ui_troll(unwind_interval *u)
{
  char buf[1000];

  _dump_ui_str(u, buf, sizeof(buf));

  PMSG_LIMIT(TMSG(TROLL,buf));
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

