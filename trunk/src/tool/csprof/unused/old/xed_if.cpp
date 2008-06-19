f#include <iostream>
#include <strstream>
#include <sstream>
#include <iomanip>
using namespace std;

extern "C" {
#include "xed-interface.h"
}

#include "intervals.h"
#include "find.h"

#if 0
#include "mem.h"
// johnmc - we should include mem.h, but it defines a variable
#else
extern "C" {
void* csprof_malloc(size_t size);
};
#endif

//****************************************************************************************
// local types
//****************************************************************************************
typedef enum {HW_NONE, HW_BRANCH, HW_CALL, HW_BPSAVE, HW_SPSUB, HW_CREATE_STD} 
  hw_type;

typedef struct highwatermark_s {
  unwind_interval *uwi;
  hw_type type;
} highwatermark_t;

#define PREFER_BP_FRAME 0



//****************************************************************************************
// forward declarations
//****************************************************************************************
static void 
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval *current, 
			    unwind_interval *&next, char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval *&canonical_interval, 
			    bool bp_frames_found);

static const char *const bp_status_string(bp_loc l);

static const char *const ra_status_string(ra_loc l); 

//****************************************************************************************
// local variables
//****************************************************************************************
xed_state_t xed_machine_state = { XED_MACHINE_MODE_LONG_64, 
				  XED_ADDRESS_WIDTH_64b,
				  XED_ADDRESS_WIDTH_64b };

xed_state_t *xed_machine_state_ptr = &xed_machine_state;

// create the decoded instruction object

extern "C" {
  void xed_init(void);
  void xed_inst(void *ins);
  int xed_inst2(void *ins);
  void xed_show(void);

#include "pmsg.h"
  // turn off msgs @ compile time
  // comment out code below 
  // define DBG true to turn on all sorts of printing
#if 1
#define DBG false
#else
#define DBG true
#endif
}
  //
  // debug block
static bool idebug  = DBG;
static bool ildebug = DBG;
static bool irdebug = DBG;
static bool jdebug = DBG;
static int dump_to_stdout = 0;

#if 0
  // can't be static
static bool bp_frames_found = false;
#endif

void xed_init(void){
   // initialize the XED tables -- one time.
  xed_tables_init();
}



unwind_interval *what(xed_decoded_inst_t *xptr, char *ins, 
		      unwind_interval *current,bool &bp_just_pushed, 
		      highwatermark_t &highwatermark,
		      unwind_interval *&canonical_interval, 
		      bool &bp_frames_found)
{
  int size;

  bool done = false;
  bool next_bp_just_pushed = false;
  bool next_bp_popped      = false;
  unwind_interval *next    = NULL;

  // debug block
  bool rdebug = DBG;
#if 0
  bool idebug = DBG;
#endif
  bool fdebug = DBG;
  bool mdebug = DBG;


  const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
  for(unsigned int i=0; i < xed_inst_noperands(xi) && done == false; i++){ 
    const xed_operand_t *op =  xed_inst_operand(xi,i);
    if (rdebug){
      cerr << "  " << i << " " << xed_operand_name(op) << " ";
    }
    xed_operand_enum_t op_name = xed_operand_name(op);
    switch (op_name) {
#if 0
    case XED_OPERAND_IMM: // immediates
      {
	const xed_immdis_t& immed =  xedd.get_immed();
	if (!immed.is_present()){
	  EMSG("!! something screwy: resource is imm, but immed not present !!");
	}
	assert(immed.is_present());
	if (idebug){
	  cerr << "   --immed value = ";
	  //                immed.print_value_unsigned(cerr);

	  immed.print_value_unsigned(cerr);
	  cerr << endl;
	}
      }
      break;

    case XED_OPERAND_IMM8: // Only for the ENTER instruction
      EMSG("!! ENTER instruction IMM8 encountered, but not supposed to!!");
      break;
#endif

    case XED_OPERAND_REG0:
    case XED_OPERAND_REG1:
    case XED_OPERAND_REG2:
    case XED_OPERAND_REG3:
    case XED_OPERAND_REG4:
    case XED_OPERAND_REG5:
    case XED_OPERAND_REG6:
    case XED_OPERAND_REG7:
    case XED_OPERAND_REG8:
    case XED_OPERAND_REG9:
    case XED_OPERAND_REG10:
    case XED_OPERAND_REG11:
    case XED_OPERAND_REG12: {
      xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op_name);
      if ((regname == XED_REG_RSP) && 
	  ((xed_operand_rw(op) == XED_OPERAND_ACTION_RW) ||  
	   (xed_operand_rw(op) == XED_OPERAND_ACTION_W))) {
	if ((current->ra_status == RA_SP_RELATIVE) || 1) {
	  if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_PUSH) {
	    switch(iclass(xptr)) {
	    case XED_ICLASS_PUSH:
	      // hack: assume 64-bit mode
	      size = 8;
	      break;
	    case XED_ICLASS_PUSHFQ:
	      size = 8;
	      break;
	    case XED_ICLASS_PUSHF:
	      size = 2;
	      break;
	    case XED_ICLASS_PUSHFD:
	      size = 4;
	      break;
	    default:
	      EMSG("!! push class, but not specfic push encountered");
	      assert(0);
	    }
	    PMSG(INTV,"new interval from PUSH");
	    next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
			       // RA_SP_RELATIVE, 
			       current->ra_status,
			       current->sp_ra_pos + size,
			       current->bp_ra_pos,
			       current->bp_status,
			       current->sp_bp_pos + size,
			       current->bp_bp_pos,
			       current);
             done = true;
	  }
	  else if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_POP) {
	    switch(iclass(xptr)) {
	    case XED_ICLASS_POP:
	      // hack: assume 64-bit mode
	      size = -8;
	      break;
	    case XED_ICLASS_POPFQ:
	      size = -8;
	      break;
	    case XED_ICLASS_POPF:
	      size = -2;
	      break;
	    case XED_ICLASS_POPFD:
	      size = -4;
	      break;
	    default:
	      EMSG("!! pop class, but not specific pop");
	      assert(0);
	    }
	    PMSG(INTV,"new interval from POP");
	    next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
			       // RA_SP_RELATIVE,
			       current->ra_status,
			       current->sp_ra_pos + size,
			       current->bp_ra_pos,
			       current->bp_status,
			       current->sp_bp_pos + size,
			       current->bp_bp_pos,
			       current);
             done = true;
	  }
	  else if (xed_decoded_inst_get_category(xptr)  == XED_CATEGORY_DATAXFER){
	    const xed_operand_t* op1 = xed_inst_operand(xi,1);
	    if ((xed_operand_name(op1) == XED_OPERAND_REG) &&
		(xed_operand_reg(op1) == XED_REG_RBP)) {
	      PMSG(INTV,"Restore RSP from BP detected @%p",ins);
	      next = newinterval(ins + xed_decoded_inst_get_length(xptr),
				 RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
				 current->bp_status,current->bp_bp_pos, current->bp_bp_pos,
			 	 current);
              done = true;
	    }
	  }
	  else if (iclass_eq(xptr, XED_ICLASS_SUB) ||
		   iclass_eq(xptr, XED_ICLASS_ADD)) { 
	    const xed_operand_t* op1 = xed_inst_operand(xi,1);
	    if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
	      int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
	      long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
		    if (mdebug){
		      cerr << "sp immed arith val = "
			   << immedv
			   << endl;
		    }
		    PMSG(INTV,"newinterval from ADD/SUB immediate");
		    ra_loc istatus = current->ra_status;
		    if ((istatus == RA_STD_FRAME) && 
			iclass_eq(xptr, XED_ICLASS_SUB) &&
			(highwatermark.type != HW_SPSUB)) {
		      //---------------------------------------------------------------------------
		      // if we are in a standard frame and we see a second subtract, it is time
		      // to convert interval to a BP frame to minimize the chance we get the 
		      // wrong offset for the return address in a routine that manipulates 
		      // SP frequently (as in leapfrog_mod_leapfrog_ in the 
		      // SPEC CPU2006 benchmark 459.GemsFDTD, when compiled with PGI 7.0.3 with
		      // high levels of optimization).
		      //
		      // 9 December 2007 -- John Mellor-Crummey
		      //---------------------------------------------------------------------------
		      istatus = RA_BP_FRAME;
		    }
		    next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
				       // RA_SP_RELATIVE, 
				       istatus,
				       current->sp_ra_pos + immedv,
				       current->bp_ra_pos,
				       current->bp_status,
				       current->sp_bp_pos + immedv,
				       current->bp_bp_pos,
				       current);
	            done = true;
			if (iclass_eq(xptr, XED_ICLASS_SUB)) {
		      if (highwatermark.type != HW_SPSUB) {
			//-------------------------------------------------------------------------
			// set the highwatermark and canonical interval upon seeing the FIRST
			// subtract from SP; take no action on subsequent subtracts.
			//
			// test case: main in SPEC CPU2006 benchmark 470.lbm contains multiple 
			// subtracts from SP when compiled with PGI 7.0.3 with high levels of 
			// optimization. the first subtract from SP is to set up the frame; 
			// subsequent ones are to reserve space for arguments passed to functions.
			//
			// 9 December 2007 -- John Mellor-Crummey
			//-------------------------------------------------------------------------
			highwatermark.uwi = next;
			highwatermark.type = HW_SPSUB;
			canonical_interval = next;
		      }
		    }
	    } else {
	      if (current->ra_status != RA_BP_FRAME){
		//---------------------------------------------------------------------------
		// no immediate in add/subtract from stack pointer; switch to BP_FRAME
		//
		// 9 December 2007 -- John Mellor-Crummey
		//---------------------------------------------------------------------------
		next = newinterval(ins + xed_decoded_inst_get_length(xptr),
				   RA_BP_FRAME,
				   current->sp_ra_pos,
				   current->bp_ra_pos,
				   current->bp_status,
				   current->sp_bp_pos,
				   current->bp_bp_pos,
				   current);
		bp_frames_found = true;
	        done = true;
		// assert(0 && "no immediate in add or sub!");
		// return &poison;
	      }
	    }
	  }
	  else{
	    if (xed_decoded_inst_get_category(xptr) != XED_CATEGORY_CALL){
	      // assert(0 && "unexpected mod of RSP!");
	      if (current->ra_status == RA_SP_RELATIVE){
		EMSG("interval: SP_RELATIVE unexpected mod of RSP @%p",ins);
		return &poison;
	      }
	    }
	  }
	}
      }
      else if (regname == XED_REG_RBP) {
	if (xed_operand_rw(op) == XED_OPERAND_ACTION_R) {
	  if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_PUSH) {
	    next_bp_just_pushed = true;
	  }
	}
	else if ((xed_operand_rw(op) == XED_OPERAND_ACTION_RW) ||  
		 (xed_operand_rw(op) == XED_OPERAND_ACTION_W)) {
	  if (fdebug){
	    cerr << "\t writes RBP" << endl;
	  }
	  if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_POP){
	    next_bp_popped = true;
	  }
          if( (current->bp_status == BP_SAVED) &&
	      (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_DATAXFER)) {
	    const xed_operand_t* op1 = xed_inst_operand(xi,1);
	    if ((xed_operand_name(op1) == XED_OPERAND_REG) && 
		(xed_operand_reg(op1) == XED_REG_RSP)) {

	      PMSG(INTV,"new interval from PUSH BP");
	      next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
#if PREFER_BP_FRAME
				 RA_BP_FRAME,
#else
				 RA_STD_FRAME,
#endif
				 current->sp_ra_pos,
				 current->sp_ra_pos,
				 BP_SAVED,
				 current->sp_bp_pos,
				 current->sp_bp_pos,
				 current); 
	      highwatermark.uwi = next;
	      highwatermark.type = HW_CREATE_STD;
	      done = true;
	    }
	  }
	  else if ((xed_decoded_inst_get_category(xptr) == XED_CATEGORY_POP) &&
		   (current->ra_status == RA_BP_FRAME)){
	    switch(iclass(xptr)) {
	    case XED_ICLASS_POP:
	      // hack: assume 64-bit mode
	      size = -8;
	      break;
	    case XED_ICLASS_POPFQ:
	      size = -8;
	      break;
	    case XED_ICLASS_POPF:
	      size = -2;
	      break;
	    case XED_ICLASS_POPFD:
	      size = -4;
	      break;
	    default:
	      EMSG("!! pop class, but no pop type !!");
	      assert(0);
	    }
	    // johnmc
	    EMSG("PROBLEM @%p: pop bp in BP_FRAME mode",ins);
	    next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
			       RA_SP_RELATIVE, 0, 0,
			       BP_UNCHANGED, current->sp_bp_pos, 0,
			       current);
	    done = true;
	  }
	} 
      }
      break;
    }
    default:
      break;

    }
  }
  bp_just_pushed = next_bp_just_pushed;
  if (bp_just_pushed){
    next->bp_status = BP_SAVED;
    next->sp_bp_pos    = 0;
  }
  if (next_bp_popped){
    next->bp_status = BP_UNCHANGED;
  }
  return (next) ? next : current;
}


// #define DEBUG 1
void set_flags(bool val)
{
  idebug  = val;
  ildebug = val;
  irdebug = val;
  jdebug = val;
}
