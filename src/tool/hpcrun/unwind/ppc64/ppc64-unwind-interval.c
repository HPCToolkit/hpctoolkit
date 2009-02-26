// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************

/*****************************************************************************
 * system include files 
 *****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>


/*****************************************************************************
 * local include files 
 *****************************************************************************/

#include <include/gcc-attr.h>

#include "ppc64-unwind-interval.h"

#include "csprof-malloc.h"
#include "pmsg.h"
#include "atomic-ops.h"
#include "ui_tree.h"

#include "fnbounds_interface.h"


//***************************************************************************
// macros
//***************************************************************************

#define STR(s) case s: return #s


#define REGISTER_R0 (-5)
#define REGISTER_LR (-3)


#define MFLR_R0(x)      ((x) == 0x7c0802a6)
#define STW_R0_R1(x)    (((x) & 0xffff0000) == 0x90010000)
#define STWU_R1_R1(x)   (((x) & 0xffff0000) == 0x94210000)
#define MTLR_R0(x)      ((x) == 0x7c0803a6)
#define LWZ_R0_R1(x)    (((x) & 0xffff0000) == 0x80010000)
#define DISP(x)         (((short)((x) & 0x0000ffff)))
#define ADDI_R1(x)      (((x) & 0xffff0000) == 0x38210000)
#define MR_R1(x)        (((x) & 0xffc003ff) == 0x7d400378)
#define BLR(x)          ((x) == 0x4e800020)

#define NEXT_INS        ((char *) (cur_ins + 1))


/*****************************************************************************
 * forward declarations
 *****************************************************************************/

static interval_status 
ppc64_build_intervals(char *ins, unsigned int len);

static void 
ppc64_print_interval_set(unwind_interval *first);

static const char *ra_status_string(ra_loc l); 
static const char *bp_status_string(bp_loc l);



//***************************************************************************
// global variables
//***************************************************************************

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



//***************************************************************************
// interface operations
//***************************************************************************

interval_status 
build_intervals(char *ins, unsigned int len)
{
  interval_status stat = ppc64_build_intervals(ins, len);

  ppc64_print_interval_set((unwind_interval *) stat.first);

  return stat;
}


//***************************************************************************
// interface operations 
//***************************************************************************

unwind_interval *
new_ui(char *start, 
       ra_loc ra_status, int bp_ra_pos, 
       bp_loc bp_status,          
       unwind_interval *prev)
{
  unwind_interval *u = (unwind_interval *) csprof_ui_malloc(sizeof(unwind_interval)); 

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

  sprintf(buf, "INTV: start=%p end =%p ra_status=%s bp_status=%s "
	  "bp_ra_pos = %d next=%p prev=%p\n", 
	  (void *) u->common.start, (void *) u->common.end, 
	  ra_status_string(u->ra_status), bp_status_string(u->bp_status), 
	  u->bp_ra_pos, u->common.next, u->common.prev); 

  PMSG(INTV,buf);
#if 0
  if (dump_to_stdout) { 
    fprintf(stderr, "%s", buf);
    fflush(stderr);
  }
#endif
}


//***************************************************************************

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


#if 0
static const char *
register_name(int reg)
{
   switch(reg) {
   case REGISTER_LR: return "lr";
   case REGISTER_R0: return "r0";
   default:          assert(0);
   }
   return NULL;
}
#endif


//***************************************************************************
// build_intervals:
//***************************************************************************

// states
//   ra in LR, BP_UNCHANGED, RA_REGISTER
//   parent's BP in R1 - BP_UNCHANGED, RA_REGISTER
//   my BP in R1 - BP_SAVED, RA_REGISTER
//   ra relative to bp, BP_SAVED, BP_FRAME

/*
  Typical PPC frames:
  
  // stwu: store with update:
  //       push the stack pointer (r1) and then update the stack pointer
  // mflr: move from LR (and store into r0)
  // stw: store word: Push LR (which contains the return address)
  stwu r1, -32(r1)
  mflr r0
  stw  r0, 36(r1)

  // ... do some stuff ...

  // Pop LR
  // mtlr: move to LR (from r0)
  // blr: return!
  lwz r0, 36(r1)
  mtlr r0
  blr
*/


static interval_status 
ppc64_build_intervals(char *ins, unsigned int len)
{
  interval_status stat;

  unwind_interval *first = new_ui(ins, RA_REGISTER, REGISTER_LR, 
				  BP_UNCHANGED,  NULL);
  unwind_interval *ui = first;
  unwind_interval *next = NULL;
  unwind_interval *canonical = first;

  int frame_size = 0;
  int ra_disp = 0;

  int *cur_ins = (int *) ins;
  int *end_ins = (int *) (ins + len);

  while (cur_ins < end_ins) {
      TMSG(INTV,"trying 0x%x [%p,%p)\n",*cur_ins, cur_ins, end_ins);
    //--------------------------------------------------
    // move return address from LR to R0
    //--------------------------------------------------
    if (MFLR_R0(*cur_ins)) {
      next = new_ui(NEXT_INS, RA_REGISTER, REGISTER_R0, ui->bp_status, ui);
      link_ui(ui, next); ui = next;
    }

    //--------------------------------------------------
    // store return address into parent's frame
    // we don't need to remember the offset for the return address
    // because we can just get it out of the parent's frame
    //--------------------------------------------------
    if (STW_R0_R1(*cur_ins)) {
      short ra_disp = DISP(*cur_ins);
      if ((frame_size + 4 == (int) ra_disp) && (ra_disp > 4)) {
        next = new_ui(NEXT_INS, RA_BP_FRAME, 0, BP_SAVED, ui);
        if (canonical == first) canonical = next;
        link_ui(ui, next); ui = next;
      }
    }

    //--------------------------------------------------
    // set up my frame
    // NOTE: return address remains in the register that 
    //       it was in before 
    //--------------------------------------------------
    if (STWU_R1_R1(*cur_ins)) {
      next = new_ui(NEXT_INS, RA_REGISTER, ui->bp_ra_pos, BP_SAVED, ui);
      frame_size = - DISP(*cur_ins);
      link_ui(ui, next); ui = next;
    }

    //--------------------------------------------------
    // move return address from R0 to LR
    //--------------------------------------------------
    if (MTLR_R0(*cur_ins)) {
      next = new_ui(NEXT_INS, RA_REGISTER, REGISTER_LR, ui->bp_status, ui);
      link_ui(ui, next); ui = next;
    }

    //--------------------------------------------------
    // reset frame pointer to caller's frame
    //--------------------------------------------------
    if (MR_R1(*cur_ins) || (ADDI_R1(*cur_ins) && (DISP(*cur_ins) == frame_size)))
    {
      // assume the mr restores the proper value; we would have to track 
      // register values to be sure.
      next = new_ui(NEXT_INS, ui->ra_status, ui->bp_ra_pos, BP_UNCHANGED, ui);
      link_ui(ui, next); ui = next;
    }

    //--------------------------------------------------
    // load return address into R0 from caller's frame
    //--------------------------------------------------
    if (LWZ_R0_R1(*cur_ins) && DISP(*cur_ins) == ra_disp) {
      next = new_ui(NEXT_INS, RA_REGISTER, REGISTER_R0, ui->bp_status, ui);
      link_ui(ui, next); ui = next;
    }
    if (BLR(*cur_ins) && (cur_ins + 1 < end_ins)) {
      // a return, but not the last instruction in the routine
      if ((ui->ra_status !=  canonical->ra_status) &&
          (ui->bp_ra_pos !=  canonical->bp_ra_pos) &&
          (ui->bp_status !=  canonical->bp_status)) {
         // don't make a new interval if it is the same as the present one.
         next = new_ui(NEXT_INS, canonical->ra_status, canonical->bp_ra_pos, 
                       canonical->bp_status, ui);
         link_ui(ui, next); ui = next;
      }
    }


    cur_ins++;
  }

  ui->common.end = end_ins;

  stat.first_undecoded_ins = NULL;
  stat.errcode = 0;
  stat.first = (splay_interval_t *) first;

  return stat; 
}


static void 
ppc64_print_interval_set(unwind_interval *first) 
{
  unwind_interval *u;

  for(u = first; u; u = (unwind_interval *) u->common.next) {
    dump_ui(u, 1);
  }
}


static void GCC_ATTR_UNUSED
ppc64_dump_intervals(char  *addr)
{
  void *s, *e;
  interval_status intervals;

  fnbounds_enclosing_addr(addr, &s, &e);

  uintptr_t llen = ((unsigned long) e) - (unsigned long) s;

  printf("build intervals from %p to %p (%"PRIuPTR")\n", s, e, llen);
  intervals = ppc64_build_intervals(s, (unsigned int) llen);

  ppc64_print_interval_set((unwind_interval *) intervals.first);
}
