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
  void idump(unwind_interval *u);

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

static unwind_interval poison = {
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

#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)
#define iclass_eq(xptr, class) (iclass(xptr) == (class))

void xed_init(void){
   // initialize the XED tables -- one time.
  xed_tables_init();
}
// wrapper to ensure a C interface
void idump(unwind_interval *u){
  dump(u);
}


bool no_push_bp_save(xed_decoded_inst_t *xptr){

  if (iclass_eq(xptr, XED_ICLASS_MOV)){
    const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
    const xed_operand_t *op0 =  xed_inst_operand(xi,0);
    const xed_operand_t *op1 =  xed_inst_operand(xi,1);

    return ((xed_operand_name(op0) == XED_OPERAND_MEM0) &&
	    (xed_operand_rw(op0) == XED_OPERAND_ACTION_W) &&
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RSP) &&
	    (xed_operand_name(op1) == XED_OPERAND_REG) &&
	    (xed_operand_reg(op1) == XED_REG_RBP) &&
	    (xed_operand_rw(op1) == XED_OPERAND_ACTION_R));
  }
  return false;
}

bool no_pop_bp_restore(xed_decoded_inst_t *xptr){
  
  if (iclass_eq(xptr, XED_ICLASS_MOV)) {
    const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
    const xed_operand_t *op0 =  xed_inst_operand(xi,0);
    const xed_operand_t *op1 =  xed_inst_operand(xi,1);

    return ((xed_operand_name(op1) == XED_OPERAND_MEM0) &&
	    (xed_operand_rw(op1) == XED_OPERAND_ACTION_R) &&
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RSP) &&
	    (xed_operand_name(op0) == XED_OPERAND_REG) &&
	    (xed_operand_reg(op0) == XED_REG_RBP) &&
	    (xed_operand_rw(op0) == XED_OPERAND_ACTION_W));
  }
  return false;
}


bool plt_is_next(char *ins)
{
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  xed_error_enum_t xed_err;
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  char *val_pushed = NULL;
  char *push_succ_addr = NULL;
  char *jmp_target = NULL;

  // skip optional padding if there appears to be any
  while ((((long) ins) & 0x11) && (*ins == 0x0)) ins++; 

  // -------------------------------------------------------
  // requirement 1: push of displacement relative to rip 
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(xptr, reinterpret_cast<const uint8_t*>(ins), 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_PUSH)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 2) {
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 = xed_inst_operand(xi, 0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RIP)) {
	int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	push_succ_addr = ins + xed_decoded_inst_get_length(xptr);
	val_pushed = push_succ_addr + offset;
      }
    }
  }

  if (val_pushed == NULL) {
    // push of proper type not recognized 
    return false;
  }

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(&xedd, reinterpret_cast<const uint8_t*>(push_succ_addr), 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) || 
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 1) {

      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi,0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RIP)) {
	long long offset = xed_decoded_inst_get_memory_displacement(xptr,0);
	jmp_target = push_succ_addr + xed_decoded_inst_get_length(xptr) + offset;
      }
    }
  }

  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return false;
  }

  if ((jmp_target - val_pushed) == 8) return true;

  return false;
}

unwind_interval *
process_leave(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next;
  next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
		     RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, current);
}

unwind_interval *
process_return(xed_decoded_inst_t *xptr, unwind_interval *&current, 
	       char *&ins, char *end, 
	       bool irdebug, unwind_interval *first, 
	       highwatermark_t *highwatermark, 
	       unwind_interval *&canonical_interval, bool bp_frames_found)
{
  unwind_interval *next = current;
  if (ins + xed_decoded_inst_get_length(xptr) < end) {
    //-------------------------------------------------------------------------
    // the return is not the last instruction in the interval; 
    // set up an interval for code after the return 
    //-------------------------------------------------------------------------
    if (plt_is_next(ins + xed_decoded_inst_get_length(xptr))) {
      //-------------------------------------------------------------------------
      // the code following the return is a program linkage table. each entry in 
      // the program linkage table should be invoked with the return address at 
      // top of the stack. this is exactly what the interval containing this
      // return instruction looks like. set the current interval as the 
      // "canonical interval" to be restored after then jump at the end of each
      // entry in the PLT. 
      //-------------------------------------------------------------------------
      canonical_interval = current;
    } else {
      reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, first, 
				  highwatermark, canonical_interval, bp_frames_found); 
    }
  }
  return next;
}

unwind_interval *
process_push(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark)
{
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  int size;

  switch(iclass(xptr)) {
  case XED_ICLASS_PUSH: size = 8; break; // hack: assume 64-bit mode
  case XED_ICLASS_PUSHFQ: size = 8; break;
  case XED_ICLASS_PUSHF: size = 2; break;
  case XED_ICLASS_PUSHFD: size = 4; break;
  default: assert(0);
  }

  next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
		     current->ra_status, current->sp_ra_pos + size,
		     current->bp_ra_pos, current->bp_status,
		     current->sp_bp_pos + size, current->bp_bp_pos,
		     current);
  {
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op_name);
    if (regname == XED_REG_RBP) {
      next->bp_status = BP_SAVED;
      next->sp_bp_pos    = 0;
    }
  }

  return next;
}

unwind_interval *
process_move(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next = current;
  if (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RSP) {
    //-------------------------------------------------------------------------
    // a memory move with SP as a base register
    //-------------------------------------------------------------------------
    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

    xed_operand_enum_t   op0_name = xed_operand_name(op0);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);
    
    if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) { 
      //-------------------------------------------------------------------------
      // storing a register to memory 
      //-------------------------------------------------------------------------
      if (xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RBP)  {
	//-------------------------------------------------------------------------
	// register being stored is BP
	//-------------------------------------------------------------------------
	if (current->bp_status != BP_SAVED){
	  //=========================================================================
	  // instruction: save caller's BP into the stack  
	  // action:      create a new interval with 
	  //                (1) BP status reset to BP_SAVED
	  //                (2) BP position relative to the stack pointer set to the
	  //                    offset from SP 
	  //=========================================================================
	  next = newinterval(ins + xed_decoded_inst_get_length(xptr),
			     current->ra_status,current->sp_ra_pos,current->bp_ra_pos,
			     BP_SAVED,
			     xed_decoded_inst_get_memory_displacement(xptr, 0),
			     current->bp_bp_pos,
			     current);
	  highwatermark->uwi = next;
	  highwatermark->type = HW_BPSAVE;
	}
      }
    } else if ((op1_name == XED_OPERAND_MEM0) && (op0_name == XED_OPERAND_REG0)) { 
      //-------------------------------------------------------------------------
      // loading a register from memory 
      //-------------------------------------------------------------------------
      if (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RBP) {
	//-------------------------------------------------------------------------
	// register being loaded is BP
	//-------------------------------------------------------------------------
	if (current->bp_status == BP_SAVED) {
	  int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	  if (offset == current->sp_bp_pos) { 
	    //=========================================================================
	    // instruction: restore BP from its saved location in the stack  
	    // action:      create a new interval with BP status reset to BP_UNCHANGED
	    //=========================================================================
	    unwind_interval *next; 
	    next = newinterval(ins + xed_decoded_inst_get_length(xptr),
			       current->ra_status, current->sp_ra_pos, current->bp_ra_pos,
			       BP_UNCHANGED, current->sp_bp_pos, current->bp_bp_pos,
			       current);
	  }
	}
      }
    }
  } else { 
    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

    xed_operand_enum_t   op0_name = xed_operand_name(op0);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);

    if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)) { 
      //-------------------------------------------------------------------------
      // register-to-register move 
      //-------------------------------------------------------------------------
      if ((xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RBP) &&
	  (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RSP)) {
	//=========================================================================
	// instruction: restore SP from BP
	// action:      begin a new SP_RELATIVE interval 
	//=========================================================================
	next = newinterval(ins + xed_decoded_inst_get_length(xptr),
			   RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
			   current->bp_status,current->bp_bp_pos, current->bp_bp_pos,
			   current);
      } else if ((xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RBP) &&
		 (xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RSP)) {
	//=========================================================================
	// instruction: initialize BP with value of SP to set up a frame pointer
	// action:      begin a new SP_RELATIVE interval 
	//=========================================================================
	next = newinterval(ins + xed_decoded_inst_get_length(xptr), 
#if PREFER_BP_FRAME
			   RA_BP_FRAME,
#else
			   RA_STD_FRAME,
#endif
			   current->sp_ra_pos, current->sp_ra_pos, BP_SAVED,
			   current->sp_bp_pos, current->sp_bp_pos, current); 
	highwatermark->uwi = next;
	highwatermark->type = HW_CREATE_STD;
      }
    }
  }
  return next;
}

unwind_interval *
process_enter(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next;

  long offset = 8;

  for(unsigned int i=0; i < xed_inst_noperands(xi) ; i++) {
    const xed_operand_t *op =  xed_inst_operand(xi,i);
    switch (xed_operand_name(op)) {
    case XED_OPERAND_IMM0SIGNED:
      offset += xed_decoded_inst_get_signed_immediate(xptr);
      break;
#if 0
      // johnmc - FIXME - need to handle this, but need to see what instruction looks like to figure out how.
    case XED_OPERAND_IMM8:
      {
	int tmp = (int) r.get_imm8();
	offset += 8 * ((tmp >= 1) ? (tmp -1):tmp);
      }
      break;
#endif
    default:
      // RSP, RBP ...
      break;
    }
  }
  PMSG(INTV,"new interval from ENTER");
  next = newinterval(ins + xed_decoded_inst_get_length(xptr),
#if PREFER_BP_FRAME
		     RA_BP_FRAME,
#else
		     RA_STD_FRAME,
#endif
		     current->sp_ra_pos + offset, 8, BP_SAVED,
		     current->sp_bp_pos + offset - 8, 0, current);
  highwatermark->uwi = next;
  highwatermark->type = HW_BPSAVE;
  return next;
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

  // johnmc: don't consider this a save of the caller's RBP if the
  // caller's RBP has already been saved. (change 2)
  if (no_push_bp_save(xptr) && current->bp_status != BP_SAVED){
    next = newinterval(ins + xed_decoded_inst_get_length(xptr),
		       current->ra_status,current->sp_ra_pos,current->bp_ra_pos,
		       BP_SAVED,
		       xed_decoded_inst_get_memory_displacement(xptr, 0),
		       current->bp_bp_pos,
		       current);
    highwatermark.uwi = next;
    highwatermark.type = HW_BPSAVE;
    return next;
  }
  if (current->bp_status == BP_SAVED && no_pop_bp_restore(xptr)) {
    int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
    if (offset == current->sp_bp_pos) { 
      // we are restoring BP from its saved location, restoring BP status to BP_UNCHANGED
      next = newinterval(ins + xed_decoded_inst_get_length(xptr),
			 current->ra_status,current->sp_ra_pos,current->bp_ra_pos,
			 BP_UNCHANGED,current->sp_bp_pos,current->bp_bp_pos,
			 current);
      return next;
    }
  }
  // FIXME: look up bp action f enter
  if (iclass_eq(xptr, XED_ICLASS_ENTER)) {
    long offset = 8;

    const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
    for(unsigned int i=0; i < xed_inst_noperands(xi) ; i++) {
      const xed_operand_t *op =  xed_inst_operand(xi,i);
      switch (xed_operand_name(op)) {
      case XED_OPERAND_IMM0SIGNED:
	offset += xed_decoded_inst_get_signed_immediate(xptr);
	break;
#if 0
	// johnmc - FIXME - need to handle this, but need to see what instruction looks like to figure out how.
      case XED_OPERAND_IMM8:
	{
	int tmp = (int) r.get_imm8();
	offset += 8 * ((tmp >= 1) ? (tmp -1):tmp);
	}
	break;
#endif
      default:
	// RSP, RBP ...
	break;
      }
    }
    PMSG(INTV,"new interval from ENTER");
    next = newinterval(ins + xed_decoded_inst_get_length(xptr),

#define PREFER_BP_FRAME 0
#if PREFER_BP_FRAME
		       RA_BP_FRAME,
#else
		       RA_STD_FRAME,
#endif
		       current->sp_ra_pos + offset,
		       8,
		       BP_SAVED,
		       current->sp_bp_pos + offset -8,
		       0,
		       current);
    highwatermark.uwi = next;
    highwatermark.type = HW_BPSAVE;
    return next;
  }
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


unwind_interval *process_inst(xed_decoded_inst_t *xptr, char *ins, char *end, 
			      unwind_interval *current, unwind_interval *&first,
			      bool &bp_just_pushed, 
			      highwatermark_t &highwatermark,
			      unwind_interval *&canonical_interval, 
			      bool &bp_frames_found){

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  unwind_interval *next;

  switch(xiclass) {
  case XED_ICLASS_MOV: 
    next = process_move(ins, xptr, xi, current, &highwatermark);
  case XED_ICLASS_ENTER:
    next = process_enter(ins, xptr, xi, current, &highwatermark);
    break;
  case XED_ICLASS_LEAVE:
    next = process_leave(ins, xptr, xi, current, &highwatermark);
    break;
  case XED_ICLASS_IRET:
  case XED_ICLASS_IRETD:
  case XED_ICLASS_IRETQ:
  case XED_ICLASS_RET_FAR:
  case XED_ICLASS_RET_NEAR:
    next = process_return(xptr, current, ins, end, irdebug, first, &highwatermark, 
			  canonical_interval, bp_frames_found);
    break;
  case XED_ICLASS_PUSH:   
  case XED_ICLASS_PUSHFQ: 
  case XED_ICLASS_PUSHF:  
  case XED_ICLASS_PUSHFD: 
    next = process_push(ins, xptr, xi, current, &highwatermark);
    break;
  default:
    next = what(xptr, ins, current, bp_just_pushed, highwatermark, canonical_interval, 
		bp_frames_found);
  }
}


// #define DEBUG 1

unwind_interval *find_first_bp_frame(unwind_interval *first){
  while (first && (first->ra_status != RA_BP_FRAME)) first = first->next;
  return first;
}

unwind_interval *find_first_non_decr(unwind_interval *first, 
				     unwind_interval *highwatermark){

  while (first && first->next && (first->sp_ra_pos <= first->next->sp_ra_pos) && 
	 (first != highwatermark)) {
    first = first->next;
  }
  return first;
}

static void 
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval *current, 
			    unwind_interval *&next, char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval *&canonical_interval, 
			    bool bp_frames_found)
{

  unwind_interval *hw_uwi = highwatermark->uwi;
  // if the return is not the last instruction in the interval, 
  // set up an interval for code after the return 
  if (ins + xed_decoded_inst_get_length(xptr) < end){
    if (canonical_interval) {
      if ((hw_uwi && hw_uwi->bp_status == BP_SAVED) && 
	  (canonical_interval->bp_status != BP_SAVED) &&
	  (canonical_interval->sp_ra_pos == hw_uwi->sp_ra_pos))
	canonical_interval = hw_uwi;
      first = canonical_interval;
    } else if (bp_frames_found){ 
      // look for first bp frame
      first = find_first_bp_frame(first);
      canonical_interval = first;
    } else { 
      // look for first nondecreasing with no jmp
      first = find_first_non_decr(first, hw_uwi);
      canonical_interval = first;
    }
    PMSG(INTV,"new interval from RET");
#if 0

    ra_loc istatus =  
      (first->ra_status == RA_STD_FRAME) ? RA_BP_FRAME : first->ra_status;
#else
    ra_loc istatus = first->ra_status;
#endif
    if ((current->ra_status != istatus) ||
	(current->bp_status != first->bp_status) ||
	(current->sp_ra_pos != first->sp_ra_pos) ||
	(current->bp_ra_pos != first->bp_ra_pos) ||
	(current->bp_bp_pos != first->bp_bp_pos) ||
	(current->sp_bp_pos != first->sp_bp_pos)) {
      next = newinterval(ins + xed_decoded_inst_get_length(xptr),
			 istatus,first->sp_ra_pos,first->bp_ra_pos,
			 first->bp_status,first->sp_bp_pos,first->bp_bp_pos,
			 current);
      return;
    }
  }
  next = current; 
}

void set_flags(bool val)
{
  idebug  = val;
  ildebug = val;
  irdebug = val;
  jdebug = val;
}
void pl_build_intervals(char  *addr, int xed, int pint) 
{
	char *s, *e;
	unwind_interval *u;
	find_enclosing_function_bounds(addr, &s, &e);

	interval_status intervals;
	set_flags(xed);
	intervals = l_build_intervals(s, e - s);
	set_flags(false);
	dump_to_stdout = pint;
	_dbg_pmsg_stderr();
	for(u = intervals.first; u; u = u->next) {
		dump(u);
	}
	dump_to_stdout = 0;
	_dbg_pmsg_restore();
}

xed_decoded_inst_t _dbg_xedd;

void pl_xedd(void *ins){
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_decode(xptr, reinterpret_cast<const uint8_t*>(ins), 15);
}

void pl_dump_ins(void *ins){
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_error = xed_decode(xptr, reinterpret_cast<const uint8_t*>(ins), 15);

  cerr << (void *) ins << " (" << xed_decoded_inst_get_length(xptr) << " bytes) " 
       << iclass(xptr) << endl;
  print_operands(xptr);
  print_memops(xptr);
}

int is_jump(xed_decoded_inst_t *xptr)
{
  switch (iclass(xptr)) {
  case XED_ICLASS_JBE: 
  case XED_ICLASS_JL:
  case XED_ICLASS_JLE:
  case XED_ICLASS_JMP:
  case XED_ICLASS_JMP_FAR:
  case XED_ICLASS_JNB:
  case XED_ICLASS_JNBE:
  case XED_ICLASS_JNL:
  case XED_ICLASS_JNLE:
  case XED_ICLASS_JNO:
  case XED_ICLASS_JNP:
  case XED_ICLASS_JNS:
  case XED_ICLASS_JNZ:
  case XED_ICLASS_JO:
  case XED_ICLASS_JP:
  case XED_ICLASS_JRCXZ:
  case XED_ICLASS_JS:
  case XED_ICLASS_JZ:
     return true;
  default:
     return false;
  }
  return false;
}

#undef USE_CALL_LOOKAHEAD
#ifdef USE_CALL_LOOKAHEAD
unwind_interval *
call_lookahead(xed_decoded_inst_t *call_xedd, unwind_interval *current, char *ins)
{
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  unwind_interval *next;
  xed_error_enum_t xed_err;
  int length = call_xedd->get_length();
  xed_decoded_inst_t xeddobj;
  xed_decoded_inst_t* xedd = &xeddobj;
  char *jmp_ins_addr = ins + length;
  char *jmp_target = NULL;
  char *jmp_succ_addr = NULL;

  if (current->ra_status == RA_BP_FRAME) {
    return current;
  }

  // -------------------------------------------------------
  // requirement 1: unconditional jump with known target within routine
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(xptr, reinterpret_cast<const uint8_t*>(jmp_ins_addr), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) || 
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 0) {
      const xed_immdis_t& disp = xptr->get_disp();
      if (disp.is_present()) {
	long long offset = disp.get_signed64();
	jmp_succ_addr = jmp_ins_addr + xptr->get_length();
	jmp_target = jmp_succ_addr + offset;
      }
    }
  }
  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return current;
  }
  // FIXME: possibly test to ensure jmp_target is within routine

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(xptr, reinterpret_cast<const uint8_t*>(jmp_target), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }
  
  if (iclass_eq(xptr, XED_ICLASS_SUB) || iclass_eq(xptr, XED_ICLASS_ADD)) {
    const xed_operand_t* op0 = xed_inst_operand(xi,0);
    if ((xed_operand_name(op0) == XED_OPERAND_REG) && (xed_operand_reg(op0) == XED_REG_RSP)) {
      const xed_immdis_t& immed = xptr->get_immed();
      if (immed.is_present()) {
	int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
	long offset = sign * immed.get_signed64();
	PMSG(INTV,"newinterval from ADD/SUB immediate");
	next = newinterval(jmp_succ_addr,
			   current->ra_status,
			   current->sp_ra_pos + offset,
			   current->bp_ra_pos,
			   current->bp_status,
			   current->sp_bp_pos + offset,
			   current->bp_bp_pos,
			   current);
        return next;
      }
    }
  }
  return current;
}
#endif


interval_status l_build_intervals(char *ins, unsigned int len)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;

  interval_status xed_stat;
  xed_error_enum_t xed_error;
  unwind_interval *prev = NULL, *current = NULL, *next = NULL, *first = NULL;
  highwatermark_t highwatermark = { 0, HW_NONE };
  unwind_interval *canonical_interval = 0;
  bool bp_just_pushed = false;
  int ecnt = 0;


  bool bp_frames_found = false;  // handle return is different if there are any bp frames

  char *start = ins;
  char *end = ins + len;


  PMSG(INTV,"L_BUILD: start = %p, end = %p",start,end);
  current = newinterval(ins, RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, NULL);

  if (ildebug){
    PMSG(INTV,"L_BUILD:dump first");
    dump(current);
  }

  first = current;
  while (ins < end) {
    
    // fill in the machine state
    xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);

    PMSG(INTV,"L_BUILD: in loop, ins = %p",ins);
    xed_error = xed_decode(xptr, reinterpret_cast<const uint8_t*>(ins), 15);
    switch(xed_error) {
    case XED_ERROR_NONE:
      if (idebug){
	PMSG(INTV,"trying to dump an instruction!!!");
	cerr << (void *) ins << " (" << xed_decoded_inst_get_length(xptr) << " bytes) " 
	     << iclass(xptr) << endl;
      }
      PMSG(INTV2,"%p yielded ok decode",ins);
      break;
    case XED_ERROR_BUFFER_TOO_SHORT:
      PMSG(INTV,"Reached buffer too short error");
    case XED_ERROR_GENERAL_ERROR:
      PMSG(INTV,"Reached General error");
    default:
      // Forge ahead, even though we know this is not a valid instruction
      // MWF + JMC
      //
      PMSG(INTV,"taking default incr action");
      ins++;
      ecnt++;
      continue;
      // goto done;
    }
    if (iclass_eq(xptr, XED_ICLASS_LEAVE)) {
      PMSG(INTV,"new interval from LEAVE");
      next = newinterval(ins + xed_decoded_inst_get_length(xptr), RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, current);
    } 
    else if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_RET) {
      // if the return is not the last instruction in the interval, 
      // set up an interval for code after the return 
      if (ins + xed_decoded_inst_get_length(xptr) < end) {
	if (plt_is_next(ins + xed_decoded_inst_get_length(xptr))) canonical_interval = current;
	else {
	  reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, first, 
				      &highwatermark, canonical_interval, bp_frames_found); 
	}
      }
    } 
    else if (iclass_eq(xptr, XED_ICLASS_JMP) || iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
      if (highwatermark.type == HW_NONE)  {
	highwatermark.uwi = current; 
	highwatermark.type = HW_BRANCH; 
      }

#define RESET_FRAME_FOR_ALL_UNCONDITIONAL_JUMPS
#ifdef  RESET_FRAME_FOR_ALL_UNCONDITIONAL_JUMPS
      reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, first, 
				  &highwatermark, canonical_interval, bp_frames_found); 
#else
      if (xed_decoded_inst_number_of_memory_operands(xptr) == 0) {
	const xed_immdis_t& disp =  xed_operand_values_get_displacement_for_memop(xptr);
	xed_operand_values_t *xopv;
	if (xed_operand_values_has_memory_displacement(xopv)) {
	  long long offset = 
	    xed_operand_values_get_memory_displacement_int64(xopv);
	  char *target = ins + offset;
	  if (jdebug) {
	    xedd.dump(cout);
	    cerr << "JMP offset = " << offset << ", target = " << (void *) target << ", start = " << (void *) start << ", end = " << (void *) end << endl;
	  } 
	  if (target < start || target > end) {
	    reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, 
					first, &highwatermark, canonical_interval, bp_frames_found); 
	  }
	}
	if (xedd.get_operand_count() >= 1) { 
	  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
	  const xed_operand_t *op0 =  xed_inst_operand(xi,0);
	  if ((xed_operand_name(op0) == XED_OPERAND_REG) && 
	      (current->ra_status == RA_SP_RELATIVE) && 
	      (current->sp_ra_pos == 0)) {
	    // jump to a register when the stack is in an SP relative state
	    // with the return address at offset 0 in the stack.
	    // a good assumption is that this is a tail call. -- johnmc
	    // (change 1)
	    reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, 
					first, &highwatermark, canonical_interval, bp_frames_found); 
	  }
	}
      }
#endif
    } else if (xed_decoded_inst_get_category(xptr) == XED_CATEGORY_CALL) {

      if (highwatermark.type == HW_NONE) {
	highwatermark.uwi = current;
	highwatermark.type = HW_CALL;
      }

#ifdef USE_CALL_LOOKAHEAD
      next = call_lookahead(xptr, current, ins);
#endif
    } else if (is_jump(xptr) && (highwatermark.type == HW_NONE))  {
      highwatermark.uwi = current; 
      highwatermark.type = HW_BRANCH; 
    } else {
      next = process_inst(xptr, ins, end, current, first, bp_just_pushed, highwatermark, canonical_interval, bp_frames_found);
    }
    
    if (!next) {
      next = current;
    }
   
    if (next == &poison) {
      xed_stat.first_undecoded_ins = ins;
      xed_stat.errcode 		   = -1;
      xed_stat.first   		   = NULL;
      return xed_stat;
      // break;
    }
    if (next != current) {
      link(current, next);
      prev = current;
      current = next;
      if (ildebug){
	dump(current);
      }
    }
    if (idebug){
      print_operands(xptr);
    }
    ins += xed_decoded_inst_get_length(xptr);
    current->endaddr = (unsigned long) ins;
  }

  current->endaddr = (long unsigned int) end;

  // done:
  xed_stat.first_undecoded_ins = ins;
  xed_stat.errcode = ecnt;
  xed_stat.first   = first;
  return xed_stat;
}

//****************************************************************************************
// internal functions
//****************************************************************************************

//----------------------------------------------------------------------------------------
// printing support  
//----------------------------------------------------------------------------------------
