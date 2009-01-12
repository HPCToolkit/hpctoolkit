/******************************************************************************
 * global include files
 *****************************************************************************/

#include <stdio.h>
#include <assert.h>



/******************************************************************************
 * local include files
 *****************************************************************************/

#include "pmsg.h"
#include "ppc64-unwind-interval.h"
#include "fnbounds_interface.h"



/******************************************************************************
 * macros
 *****************************************************************************/

#ifndef NULL
#define NULL 0
#endif

#define REGISTER_LR -3
#define REGISTER_R0 -5

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



/******************************************************************************
 * states
 *	ra in LR, BP_UNCHANGED, RA_REGISTER
 *	parent's BP in R1 - BP_UNCHANGED, RA_REGISTER
 *	my BP in R1 - BP_SAVED, RA_REGISTER
 *	ra relative to bp, BP_SAVED, BP_FRAME
 *****************************************************************************/


/******************************************************************************
 * interface operations 
 *****************************************************************************/

const char *
register_name(int reg)
{
   switch(reg) {
   case REGISTER_LR: return "lr";
   case REGISTER_R0: return "r0";
   default:          assert(0);
   }
   return NULL;
}


interval_status 
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
#if 1
    printf("trying 0x%x [%p,%p)\n",*cur_ins, cur_ins, end_ins);
#endif
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


void 
ppc64_dump_intervals(char  *addr) 
{
  void *s, *e;
  unwind_interval *u;
  interval_status intervals;

  fnbounds_enclosing_addr(addr, &s, &e);

  unsigned long llen = ((unsigned long) e) - (unsigned long) s;

  printf("build intervals from %p to %p %d\n",s,e,llen);
  intervals = ppc64_build_intervals(s, (unsigned int) llen);

  ppc64_print_interval_set((unwind_interval *) intervals.first);
}


interval_status 
build_intervals(char *ins, unsigned int len)
{
  interval_status stat = ppc64_build_intervals(ins, len);

  ppc64_print_interval_set((unwind_interval *) stat.first);

  return stat;
}

