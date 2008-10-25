/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string>

extern "C" {
#include "xed-interface.h"
};

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"



/******************************************************************************
 * macros
 *****************************************************************************/

#define is_call_iclass(x) \
   ((x == XED_ICLASS_CALL_FAR) || (x == XED_ICLASS_CALL_NEAR))



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_call(char *ins, long offset, xed_decoded_inst_t *xptr,
			 void *start, void *end);

static void process_branch(char *ins, long offset, xed_decoded_inst_t *xptr);

static void after_unconditional(char *ins, long offset, xed_decoded_inst_t *xptr);

static bool invalid_routine_start(unsigned char *ins);

static void addsub(char *ins, xed_decoded_inst_t *xptr, xed_iclass_enum_t iclass, 
		   long ins_offset);

static void process_move(char *ins, xed_decoded_inst_t *xptr, long ins_offset);

static void process_push(char *ins, xed_decoded_inst_t *xptr, long ins_offset);

static void process_pop(char *ins, xed_decoded_inst_t *xptr, long ins_offset);

static void process_enter(char *ins, long ins_offset);

static void process_leave(char *ins, long ins_offset);

static bool bkwd_jump_into_protected_range(char *ins, long offset, 
					   xed_decoded_inst_t *xptr);

static bool validate_tail_call_from_jump(char *ins, long offset, 
					 xed_decoded_inst_t *xptr);

/******************************************************************************
 * local variables 
 *****************************************************************************/

static xed_state_t xed_machine_state_x86_64 = { XED_MACHINE_MODE_LONG_64, 
						XED_ADDRESS_WIDTH_64b,
						XED_ADDRESS_WIDTH_64b };

static char *prologue_start = NULL;
static char *set_rbp = NULL;
static char *push_rbp = NULL;


/******************************************************************************
 * interface operations 
 *****************************************************************************/

void
process_range_init()
{
  xed_tables_init();
}

#define RELOCATE(u, offset) (((char *) (u)) - (offset)) 

void 
process_range(long offset, void *vstart, void *vend, bool fn_discovery)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  int error_count = 0;
  char *ins = (char *) vstart;
  char *end = (char *) vend;
  vector<void *> fstarts;
  entries_in_range(ins + offset, end + offset, fstarts);
  

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);
  xed_iclass_enum_t prev_xiclass = XED_ICLASS_INVALID;

  void **fstart = &fstarts[0];
  char *guidepost = RELOCATE(*fstart, offset);

  while (ins < end) {

    if (ins >= guidepost) {
      if (ins > guidepost) {
	//--------------------------------------------------------------
	// we missed a guidepost; disassembly was not properly aligned.
	// realign ins  to match the guidepost
	//--------------------------------------------------------------
#ifdef DEBUG_GUIDEPOST
	printf("resetting ins to guidepost %p from %p\n", 
	       guidepost + offset, ins + offset);
#endif
	ins = guidepost;
      }
      //----------------------------------------------------------------
      // all is well; our disassembly is properly aligned.
      // advance to the next guidepost
      //
      // NOTE: since the vector of fstarts contains end, 
      // the fstart pointer will never go past the end of the 
      // fstarts array.
      //----------------------------------------------------------------
      fstart++; guidepost = RELOCATE(*fstart, offset);
    }

    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      error_count++; /* note the error      */
      ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
    switch(xiclass) {
    case XED_ICLASS_ADD:
    case XED_ICLASS_SUB:
      addsub(ins, xptr, xiclass, offset);
      break;
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      /* if (fn_discovery) */ process_call(ins, offset, xptr, vstart, vend);
      break;

    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
      if (xed_decoded_inst_noperands(xptr) == 2) {
	const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
	const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
	const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
	xed_operand_type_enum_t op0_type = xed_operand_type(op0);
	xed_operand_type_enum_t op1_type = xed_operand_type(op1);
	if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_operand_name(op1) == XED_OPERAND_REG0) && 
	    (xed_operand_nonterminal_name(op1) == XED_NONTERMINAL_RIP)) {
	  // idiom for GOT indexing in PLT 
	  // don't consider the instruction afterward a potential function start
	  break;
	}
	if ((xed_operand_name(op0) == XED_OPERAND_REG0) && 
	    (xed_operand_name(op1) == XED_OPERAND_REG1) && 
	    (xed_operand_nonterminal_name(op1) == XED_NONTERMINAL_RIP)) {
	  // idiom for a switch using a jump table: 
	  // don't consider the instruction afterward a potential function start
	  break;
	}
      }
      bkwd_jump_into_protected_range(ins, offset, xptr);
      if (fn_discovery  && !is_call_iclass(prev_xiclass)) {
        // regarding use of !is_call above: don't infer function start 
        // if we run into code from C++ that consists of a call followed 
        // by an unconditional jump
	if (validate_tail_call_from_jump(ins, offset, xptr)) {
	  after_unconditional(ins, offset, xptr);
	}
      }
      break;
    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR:
      if (fn_discovery) after_unconditional(ins, offset, xptr);
      break;

    case XED_ICLASS_JB:
    case XED_ICLASS_JBE: 
    case XED_ICLASS_JL: 
    case XED_ICLASS_JLE: 
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
      if (fn_discovery) process_branch(ins, offset , xptr);
      break;

    case XED_ICLASS_PUSH: 
    case XED_ICLASS_PUSHFQ: 
    case XED_ICLASS_PUSHFD: 
    case XED_ICLASS_PUSHF:  
      process_push(ins, xptr, offset);
      break;

    case XED_ICLASS_POP:   
    case XED_ICLASS_POPF:  
    case XED_ICLASS_POPFD: 
    case XED_ICLASS_POPFQ: 
      process_pop(ins, xptr, offset);
      break;

    case XED_ICLASS_ENTER:
      process_enter(ins, offset);
      break;

    case XED_ICLASS_MOV: 
      process_move(ins, xptr, offset);
      break;

    case XED_ICLASS_LEAVE:
      process_leave(ins, offset);
      break;

    default:
      break;
    }
    prev_xiclass = xiclass;

    ins += xed_decoded_inst_get_length(xptr);
  }
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static int 
is_padding(int c)
{
  return (c == 0x66) || (c == 0x90);
}


//----------------------------------------------------------------------------
// code that is unreachable after a return or an unconditional jump is a 
// potential function entry point. try to screen out the cases that don't 
// make sense. infer function entry points for the rest.
//----------------------------------------------------------------------------
static void 
after_unconditional(char *ins, long offset, xed_decoded_inst_t *xptr)
{
  ins += xed_decoded_inst_get_length(xptr);
  unsigned char *uins = (unsigned char *) ins;
  unsigned char *potential_function_addr = uins + offset;
  for (; is_padding(*uins); uins++); // skip remaining padding 

  //--------------------------------------------------------------------
  // only infer a function entry before padding bytes if there isn't 
  // already one after padding bytes
  //--------------------------------------------------------------------
  if (contains_function_entry(uins + offset) == false) {
    if (!invalid_routine_start(uins)) {
      add_stripped_function_entry(potential_function_addr); 
    }
  }
}


static void *
get_branch_target(char *ins, xed_decoded_inst_t *xptr, 
		  xed_operand_values_t *vals)
{
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  int offset = 0;
  switch(bytes) {
  case 1:
    offset = (signed char) xed_operand_values_get_branch_displacement_byte(vals,0);
    break;
  case 4:
    offset = xed_operand_values_get_branch_displacement_int32(vals);
    break;
  default:
    assert(0);
  }
  char *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  char *target = end_of_call_inst + offset;
  return target;
}


static void 
process_call(char *ins, long offset, xed_decoded_inst_t *xptr, 
	     void *start, void *end)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *vaddr = get_branch_target(ins + offset,xptr,vals);
      if (consider_possible_fn_address(vaddr)) {
	add_stripped_function_entry(vaddr);
      }
    }

  }
}


static bool 
bkwd_jump_into_protected_range(char *ins, long offset, xed_decoded_inst_t *xptr)
{ 
  char *relocated_ins = ins + offset; 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);
      void *start, *end;
      if (target < relocated_ins) {
	start = target;
	end = relocated_ins; 
	if (inside_protected_range(target)) {
	  add_protected_range(start, end);
	  return true;
	}
      } 
    }
  }
  return false;
}


static bool 
validate_tail_call_from_jump(char *ins, long offset, xed_decoded_inst_t *xptr)
{ 
  char *relocated_ins = ins + offset; 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);
      if (target < relocated_ins) {
	// backward jump; if this is a tail call, it should fall on a function
	// entry already in the function entries table
	if (query_function_entry(target)) return true;
      } else {
	// forward jump; if this is a tail call, it should 
	xed_decoded_inst_t xedd_tmp;
	xed_decoded_inst_t *xptr = &xedd_tmp;
	xed_error_enum_t xed_error;

	xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);
	xed_iclass_enum_t prev_xiclass = XED_ICLASS_INVALID;

	while (ins < target) {

	  xed_decoded_inst_zero_keep_mode(xptr);
	  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

	  if (xed_error != XED_ERROR_NONE) {
	    ins++;         /* skip this byte      */
	    continue;      /* continue onward ... */
	  }

	  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
	  switch(xiclass) {
	    // unconditional jump
	  case XED_ICLASS_JMP:      case XED_ICLASS_JMP_FAR:

	    // return
	  case XED_ICLASS_RET_FAR:  case XED_ICLASS_RET_NEAR:

	    // conditional branch
	  case XED_ICLASS_JB:       case XED_ICLASS_JBE: 
	  case XED_ICLASS_JL:       case XED_ICLASS_JLE: 
	  case XED_ICLASS_JNB:      case XED_ICLASS_JNBE: 
	  case XED_ICLASS_JNL:      case XED_ICLASS_JNLE: 
	  case XED_ICLASS_JNO:      case XED_ICLASS_JNP:
	  case XED_ICLASS_JNS:      case XED_ICLASS_JNZ:
	  case XED_ICLASS_JO:       case XED_ICLASS_JP:
	  case XED_ICLASS_JRCXZ:    case XED_ICLASS_JS:
	  case XED_ICLASS_JZ:
	    return true;

	  default:
	    break;
	  }
	  ins += xed_decoded_inst_get_length(xptr);
	}
      }
    }
  }
  return false;
}  


static void 
process_branch(char *ins, long offset, xed_decoded_inst_t *xptr)
{ 
  char *relocated_ins = ins + offset; 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);
      void *start, *end;
      if (target < relocated_ins) {
        unsigned char *tloc = (unsigned char *) target - offset;
        for (; is_padding(*(tloc-1)); tloc--) { 
	  // extend branch range to before padding
	  target--;
	}
	start = target;
	end = relocated_ins; 
      } else {
	start = relocated_ins;
	//-----------------------------------------------------
	// add one to ensure that the branch target is part of 
	// the "covered" range
	//-----------------------------------------------------
	end = ((char *) target) + 1; 
      }
      add_protected_range(start, end);
    }
  }
}

static int
mem_below_rsp_or_rbp(xed_decoded_inst_t *xptr, int oindex)
{
      xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, oindex);
      if (basereg == XED_REG_RBP)  {
	return 1;
      } else if (basereg == XED_REG_RSP) {
         int64_t offset = 
	   xed_decoded_inst_get_memory_displacement(xptr, oindex);
         if (offset > 0) {
	   return 1;
	 }
     } else if (basereg == XED_REG_RAX) {
	return 1;
     }
     return 0;
}

static bool
inst_accesses_callers_mem(xed_decoded_inst_t *xptr)
{
	// if the instruction accesses memory below the rsp or rbp, this is
	// not a valid first instruction for a routine. if this is the first
	// instruction in a routine, this must be manipulating values in the
	// caller's frame, not its own.
	int noperands = xed_decoded_inst_number_of_memory_operands(xptr);
	int not_my_mem = 0;
	switch (noperands) {
		case 2: not_my_mem |= mem_below_rsp_or_rbp(xptr, 1);
		case 1: not_my_mem |= mem_below_rsp_or_rbp(xptr, 0);
	        case 0: 
                   break;
		default:
		   assert(0 && "unexpected number of memory operands");
	}
	if (not_my_mem) return true;
	return false;
}

static bool
from_rax_eax(xed_decoded_inst_t *xptr)
{
  int noperands = xed_decoded_inst_noperands(xptr);
  if (noperands == 2) {
    const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);

#if 0
    if ((xed_decoded_inst_get_iclass(xptr) == XED_ICLASS_MOV) || 
	(xed_decoded_inst_get_iclass(xptr) == XED_ICLASS_MOVSXD)) {
#endif
      if ((op1_name == XED_OPERAND_REG1) && 
	  ((xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RAX) ||
	   (xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_EAX))) {
	return true;
      }
#if 0
    }
#endif
  }

  return false;
}


// prefetches are commonly outlined from loops. don't infer them as 
// function starts after unconditional control flow
static bool
is_prefetch(xed_decoded_inst_t *xptr)
{
  switch(xed_decoded_inst_get_iclass(xptr)) {
  case XED_ICLASS_PREFETCHNTA:
  case XED_ICLASS_PREFETCHT0:
  case XED_ICLASS_PREFETCHT1:
  case XED_ICLASS_PREFETCHT2:
  case XED_ICLASS_PREFETCH_EXCLUSIVE:
  case XED_ICLASS_PREFETCH_MODIFIED:
  case XED_ICLASS_PREFETCH_RESERVED:
    return true;
  default: 
    return false;
  }
}




static bool
invalid_routine_start(unsigned char *ins)
{
	xed_decoded_inst_t xdi;

	xed_decoded_inst_zero_set_mode(&xdi, &xed_machine_state_x86_64);
	xed_error_enum_t xed_error = xed_decode(&xdi, (uint8_t*) ins, 15);

	if (xed_error == XED_ERROR_NONE) { 
		if (inst_accesses_callers_mem(&xdi)) return true;
		if (from_rax_eax(&xdi)) return true;
		if (is_prefetch(&xdi)) return true;
	}

	return false;
}



void x86_dump_ins(void *ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  char inst_buf[1024];

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);
  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  xed_format_xed(xptr, inst_buf, sizeof(inst_buf), (uint64_t) ins);
  printf("(%p, %d bytes, %s) %s \n" , ins, xed_decoded_inst_get_length(xptr),
         xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xptr)), inst_buf);
}

// #define DEBUG_ADDSUB

static void
addsub(char *ins, xed_decoded_inst_t *xptr, xed_iclass_enum_t iclass, long ins_offset)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t* op0 = xed_inst_operand(xi,0);
  const xed_operand_t* op1 = xed_inst_operand(xi,1);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  static long prologue_offset = 0;

  if ((op0_name == XED_OPERAND_REG0) &&
      (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RSP)) {

    if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
      //---------------------------------------------------------------------------
      // we are adjusting the stack pointer by a constant offset
      //---------------------------------------------------------------------------
      int sign = (iclass == XED_ICLASS_ADD) ? 1 : -1;
      long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
      if (immedv < 0) {
	prologue_start = ins;
	prologue_offset = -immedv;
#ifdef DEBUG_ADDSUB
	fprintf(stderr,"prologue %ld\n", immedv);
#endif
      } else {
#ifdef DEBUG_ADDSUB
	fprintf(stderr,"epilogue %ld\n", immedv);
#endif
	if (immedv == prologue_offset) {
	  // add one to both endpoints
	  // -- ensure that add/sub in the prologue IS NOT part of the range 
	  //    (it may be the first instruction in the function - we don't want 
	  //     to prevent it from starting a function) 
	  // -- ensure that add/sub in the epilogue IS part of the range 
	  char *end = ins + 1; 
	  add_protected_range(prologue_start + ins_offset + 1, 
			      ins + ins_offset + 1);
#ifdef DEBUG_ADDSUB
	  fprintf(stderr,"range [%p, %p] offset %ld\n", 
		  prologue_start + ins_offset, end + ins_offset, immedv);
#endif
	}
      }
    }
  }
}


// don't track the push, track the move rsp to rbp or esp to ebp
static void 
process_move(char *ins, xed_decoded_inst_t *xptr, long ins_offset)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
  
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_enum_t   op1_name = xed_operand_name(op1);
  
  if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)) { 
    //-------------------------------------------------------------------------
    // register-to-register move 
    //-------------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
    if (((reg0 == XED_REG_RBP) || (reg0 == XED_REG_EBP)) &&
	((reg1 == XED_REG_RSP) || (reg1 == XED_REG_ESP))) {
      //=========================================================================
      // instruction: initialize BP with value of SP to set up a frame pointer
      //=========================================================================
      set_rbp = ins;
    }
  }
}


static void 
process_push(char *ins, xed_decoded_inst_t *xptr, long ins_offset)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (regname == XED_REG_RBP) {
      push_rbp = ins;
    }
  }
}


static void 
process_pop(char *ins, xed_decoded_inst_t *xptr, long ins_offset)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (regname == XED_REG_RBP) {
      add_protected_range(push_rbp + ins_offset + 1, ins + ins_offset + 1);
    }
  }
}


static void 
process_enter(char *ins, long ins_offset)
{
  set_rbp = ins;
}

static void 
process_leave(char *ins, long ins_offset)
{
  add_protected_range(set_rbp + ins_offset + 1, ins + ins_offset + 1);
}
