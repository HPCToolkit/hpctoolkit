// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/******************************************************************************
 * system include files
 *****************************************************************************/


#include <stdio.h>
#include <assert.h>
#include <string>

#include <include/hpctoolkit-config.h>

/******************************************************************************
 * XED include files, and conditionally the amd extended ops (amd-xop)
 *****************************************************************************/

extern "C" {
#include <include/hpctoolkit-config.h>
#include <xed-interface.h>
#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
#include "amd-xop.h"
#endif // ENABLE_XOP

// debug callable routines  -- only callable after xed_tables_init has been called
// 

  static xed_state_t dbg_xed_machine_state =
#if defined (HOST_CPU_x86_64)
    { XED_MACHINE_MODE_LONG_64, 
      XED_ADDRESS_WIDTH_64b };
#else
      { XED_MACHINE_MODE_LONG_COMPAT_32,
	  XED_ADDRESS_WIDTH_32b };
#endif

xed_iclass_enum_t
xed_iclass(char* ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;

  xed_decoded_inst_zero_set_mode(xptr, &dbg_xed_machine_state);

  xed_error_enum_t xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_error != XED_ERROR_NONE) {
    fprintf(stderr, "!! XED decode failure of insruction @ %p", ins);
    return XED_ICLASS_INVALID;
  }
  return xed_decoded_inst_get_iclass(xptr);
}

};


/******************************************************************************
 * include files
 *****************************************************************************/

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"

#include <include/hpctoolkit-config.h>
#include <lib/isa-lean/x86/instruction-set.h>


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_call(char *ins, long offset, xed_decoded_inst_t *xptr,
			 void *start, void *end);

static bool is_push_bp(char* ins);

static bool is_sub_immed_sp(char* ins, char** next);
static bool is_2step_push_bp(char* ins);
static bool contains_bp_save(char* ins);

static bool is_push_bp_seq(char* ins);

static void process_branch(char *ins, long offset, xed_decoded_inst_t *xptr, char* vstart, char* vend);

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

static bool nextins_looks_like_fn_start(char *ins, long offset, 
					xed_decoded_inst_t *xptrin);

static bool lea_has_zero_offset(xed_decoded_inst_t *xptr);

#define RELOCATE(u, offset) (((char *) (u)) - (offset)) 


/******************************************************************************
 * local variables 
 *****************************************************************************/

static xed_state_t xed_machine_state =
#if defined (HOST_CPU_x86_64)
  { XED_MACHINE_MODE_LONG_64, 
    XED_ADDRESS_WIDTH_64b };
#else
  { XED_MACHINE_MODE_LONG_COMPAT_32,
    XED_ADDRESS_WIDTH_32b };
#endif

static char *prologue_start = NULL;
static char *set_rbp = NULL;
static char *push_rbp = NULL;
static char *push_other = NULL;
static char *last_bad = NULL;
static xed_reg_enum_t push_other_reg;


/******************************************************************************
 * Debugging Macros
 *****************************************************************************/

// Uncomment (e.g. activate) the following definition
// to access instruction byte stream.
// (Most routines taking the "ins" argument use a relocated address. Maintaining
// the rel_offset variable permits access to the actual instruction byte stream)
//

// #define DBG_INST_STRM

// Uncomment (e.g. activate) the following definition
// to get diagnostic print out of instruction stream when branch target offset is
// 2 bytes.
//

// #define DBG_BR_TARG_2

#ifdef DBG_INST_STRM

static size_t rel_offset = 0;

#  define SAVE_REL_OFFSET(offset) rel_offset = offset
#  define KILL_REL_OFFSET() rel_offset = 0

#else
#  define SAVE_REL_OFFSET(offset)
#  define KILL_REL_OFFSET()

#endif // DBG_INST_STRM




/******************************************************************************
 * interface operations 
 *****************************************************************************/

void
process_range_init()
{
  xed_tables_init();
}


void 
process_range(long offset, void *vstart, void *vend, DiscoverFnTy fn_discovery)
{
  if (fn_discovery == DiscoverFnTy_None) {
    return;
  }

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  int error_count = 0;
  char *ins = (char *) vstart;
  char *end = (char *) vend;
  vector<void *> fstarts;
  entries_in_range(ins + offset, end + offset, fstarts);
  
  void **fstart = &fstarts[0];
  char *guidepost = RELOCATE(*fstart, offset);

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);

#ifdef DEBUG
  xed_iclass_enum_t prev_xiclass = XED_ICLASS_INVALID;
#endif

  while (ins < end) {

#ifdef DEBUG
    // vins = virtual address of instruction as we expect it in the object file
    // (this is useful because we can set a breakpoint to watch a vins)
    char *vins = ins + offset; 
#endif

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
#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
      amd_decode_t decode_res;
      adv_amd_decode(&decode_res, (uint8_t*) ins);
      if (decode_res.success) {
	if (decode_res.weak) {
	  // keep count of successes that are not robust
	}
	ins += decode_res.len;
	continue;
      }
#endif // ENABLE_XOP && HOST_CPU_x86_64
      last_bad = ins;
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
      /* if (fn_discovery == DiscoverFnTy_Aggressive) */
      process_call(ins, offset, xptr, vstart, vend);
      break;

    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
      if (xed_decoded_inst_noperands(xptr) == 2) {
	const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
	const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
	const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
	//xed_operand_type_enum_t op0_type = xed_operand_type(op0); // unused
	//xed_operand_type_enum_t op1_type = xed_operand_type(op1); // unused
	if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_operand_name(op1) == XED_OPERAND_REG0) && 
	    x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 1))) {
	  // idiom for GOT indexing in PLT 
	  // don't consider the instruction afterward a potential function start
	  break;
	}
	if ((xed_operand_name(op0) == XED_OPERAND_REG0) && 
	    (xed_operand_name(op1) == XED_OPERAND_REG1) && 
	    x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 1))) {
	  // idiom for a switch using a jump table: 
	  // don't consider the instruction afterward a potential function start
	  break;
	}
      }
      bkwd_jump_into_protected_range(ins, offset, xptr);
      if (fn_discovery && 
	  (validate_tail_call_from_jump(ins, offset, xptr) || 
	   nextins_looks_like_fn_start(ins, offset, xptr))) {
        after_unconditional(ins, offset, xptr);
      }
      break;
    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR:
      if (fn_discovery == DiscoverFnTy_Aggressive) {
	after_unconditional(ins, offset, xptr);
      }
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
      if (fn_discovery == DiscoverFnTy_Aggressive) {
	process_branch(ins, offset , xptr, (char*) vstart, (char*) vend);
      }
      break;

    case XED_ICLASS_LOOP:
    case XED_ICLASS_LOOPE:
    case XED_ICLASS_LOOPNE:
      if (fn_discovery == DiscoverFnTy_Aggressive) {
	process_branch(ins, offset , xptr, (char*) vstart, (char*) vend);
      }
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

#ifdef DEBUG
    prev_xiclass = xiclass;
#endif

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

static bool
skip_padding(unsigned char **ins)
{
  bool notdone = true;
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t *xptr = &xedd_tmp;
  xed_error_enum_t xed_error;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);

  while(notdone) {
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) *ins, 15);

    if (xed_error != XED_ERROR_NONE) return false;

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
    switch(xiclass) {
    case XED_ICLASS_LEA: 
      if (!lea_has_zero_offset(xptr)) return false; 
      // gcc seems to use 0 offset lea as padding between functions
      // treat it as a nop
    case XED_ICLASS_NOP:  case XED_ICLASS_NOP2: case XED_ICLASS_NOP3: 
    case XED_ICLASS_NOP4: case XED_ICLASS_NOP5: case XED_ICLASS_NOP6: 
    case XED_ICLASS_NOP7: case XED_ICLASS_NOP8: case XED_ICLASS_NOP9:
      // nop: move to the next instruction
      *ins = *ins + xed_decoded_inst_get_length(xptr);
      break;
    default:
      // a valid instruction but not a nop; we are done skipping
      notdone = false;
      break;
    }
  } 
  return true; 
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

  if (inside_protected_range(ins + offset)) return;

  unsigned char *new_func_addr = (unsigned char *) ins;
  if (skip_padding(&new_func_addr) == false) {
    // false = not a valid instruction; 
    //   this can't be the start of a new function
    return;
  }
  if (contains_function_entry(new_func_addr + offset) == false) {
    if (!invalid_routine_start(new_func_addr)) {
#if 0
      fn_start = WEAK;
      if () { // push bp OR
              // save something at offset rel to sp  OR
	      // arith on sp
              //     ==> strong evidence f fn start.
      }
#endif
      if (is_push_bp_seq(ins)) {
	// consider this to be strong evidence. this case is necessary
	// for recognizing OpenMP tasks and parallel regions outlined 
	// by the icc compiler. since they aren't called directly, there
	// wasn't enough evidence to cause them to rate reporting as 
	// stripped functions with the case below.
	// johnmc 10/20/13
	add_function_entry(new_func_addr + offset, NULL, true /* isvisible */, 0);
      } else {
	add_stripped_function_entry(new_func_addr + offset); 
      }
    }
  }
}

static void *
get_branch_target(char *ins, xed_decoded_inst_t *xptr, 
		  xed_operand_values_t *vals)
{
  int offset = xed_operand_values_get_branch_displacement_int32(vals);
  char* insn_end = ins + xed_decoded_inst_get_length(xptr);
  void* target = (void*)(insn_end + offset);

#if defined(DBG_BR_TARG_2) && defined(DBG_INST_STRM)
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  
  if (bytes == 2) {
    fprintf(stderr, "Reached case 2 @ location: %p, offset32 = %x\n", ins, offset);
    fprintf(stderr, "byte seq @ %p = \n  ", ins);
    for (struct {unsigned i; unsigned char* p;} l = {0, (unsigned char*)(ins - rel_offset)};
	 l.i < xed_decoded_inst_get_length(xptr);
	 l.i++, l.p++) {
      fprintf(stderr, "%x  ", *l.p);
    }
    fprintf(stderr, "\n");
  }
#endif // DBG_BR_TARG_2 && DBG_INST_STRM

  return target;
}


//
// special purpose check for some flavor of 'push bp' instruction
//
static bool
is_push_bp(char* ins)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t *xptr = &xedd_tmp;
  xed_error_enum_t xed_error;


  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_decoded_inst_zero_keep_mode(xptr);

  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  if (xed_error != XED_ERROR_NONE) return false;

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

  switch(xiclass) {
  case XED_ICLASS_PUSH: 
  case XED_ICLASS_PUSHFQ: 
  case XED_ICLASS_PUSHFD: 
  case XED_ICLASS_PUSHF:
    {
      //
      // return true if push argument == some kind of bp
      //
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 =  xed_inst_operand(xi, 0);
      xed_operand_enum_t op0_name = xed_operand_name(op0);

      if (op0_name == XED_OPERAND_REG0) {
	xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
	return x86_isReg_BP(regname);
      }
      else {
	return false;
      }
    }
    break;
  default:
    return false;
    break;
  }
}

//
// check to see if there is a 'mov bp, OFFSET[sp]' within the following WINDOW instructions
//

static const size_t WINDOW = 16; // 16 instruction window

// true if save 'bp' on stack within next window instructions
//
static bool
contains_bp_save_window(char* ins, size_t window)
{
  for (size_t n = 0; n < window; n++) {
    xed_decoded_inst_t xedd_tmp;
    xed_decoded_inst_t *xptr = &xedd_tmp;
    xed_error_enum_t xed_error;

    xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
    xed_decoded_inst_zero_keep_mode(xptr);

    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) return false;

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

    if (xiclass == XED_ICLASS_MOV) {
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 =  xed_inst_operand(xi, 0);
      xed_operand_enum_t op0_name = xed_operand_name(op0);
      const xed_operand_t* op1 = xed_inst_operand(xi,1);
      xed_operand_enum_t op1_name = xed_operand_name(op1);

      if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) { 

	xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0);
	if (x86_isReg_SP(basereg)) {
	  xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
	  if (x86_isReg_BP(reg1)) return true;
	}
      }     
    }
    ins += xed_decoded_inst_get_length(xptr);
  }
  return false;
}

// Utility routine to check for a specific window for bp save
//
static bool
contains_bp_save(char* ins)
{
  return contains_bp_save_window(ins, WINDOW);
}

//
// check for subtract from sp:
//  SIDE EFFECT: return the address of the next instruction
//
static bool
is_sub_immed_sp(char* ins, char** next)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t *xptr = &xedd_tmp;
  xed_error_enum_t xed_error;


  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_decoded_inst_zero_keep_mode(xptr);

  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  if (xed_error != XED_ERROR_NONE) return false;

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

  if (xiclass != XED_ICLASS_SUB) return false;
  //
  // return true if const amt subtracted f sp
  //
  const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t* op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t op0_name = xed_operand_name(op0);

  if (op0_name != XED_OPERAND_REG0) return false;

  xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
  const xed_operand_t* op1 = xed_inst_operand(xi,1);
  *next = ins + xed_decoded_inst_get_length(xptr);

  return (x86_isReg_SP(regname) && (xed_operand_name(op1) == XED_OPERAND_IMM0));
}

//
// 2-step-push-bp ==
//     sub SOMETHING, %sp
//      .... MAYBE SOME OTHER INSTRUCTIONS ...
//     mov bp, SOME_OFFSET[%sp]
//

static bool
is_sub_immed_prologue(char* ins)
{
  char* next = NULL;
  if (is_sub_immed_sp(ins, &next)) {
    return true;
  }
  return false;
}

static bool
is_2step_push_bp(char* ins)
{
  char* next = NULL;
  if (is_sub_immed_sp(ins, &next)) {
    return contains_bp_save(next);
  }
  return false;
}

static bool
is_push_bp_seq(char* ins)
{
  return is_push_bp(ins) ||
    contains_bp_save_window(ins, 1) ||
    is_sub_immed_prologue(ins) ||
    is_2step_push_bp(ins);
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
      int call_inst_len = xed_decoded_inst_get_length(xptr);
      void *next_inst_vaddr = ((char *)ins) + offset + call_inst_len;

      SAVE_REL_OFFSET(offset);

      void* vaddr = get_branch_target(ins + offset, xptr, vals);

      KILL_REL_OFFSET();

      if (vaddr != next_inst_vaddr && consider_possible_fn_address(vaddr)) {
	//
	// if called address is a 'push bp' sequence of instructions,
	// [ ie, either a single 'push bp', or a 'sub NNN, sp', followed by
        //   a 'mov bp, MMM[sp] ]
        // then the target address of
        // this call is considered a legitimate function start,
	// and the function entry has the 'isvisible' fields set to true
	//
	if ( is_push_bp_seq((char*)vaddr - offset) ) {
	  add_function_entry(vaddr, NULL, true /* isvisible */, 1 /* call count */);
	}
	//
	// otherwise, called address is weak function start, subject to later filtering
	//
	else {
	  add_stripped_function_entry(vaddr, 1 /* call count */);
	}
      }
    }

  }
  // some function calls don't return; we have seen the last instruction
  // in a routine be a call to a function that doesn't return
  // (e.g. _gfortrani_internal_unpack_c8 in libgfortran ends in a call to abort)
  // look to see if a function signature appears next. mark a routine start
  // if appropriate
  nextins_looks_like_fn_start(ins, offset, xptr);
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

      SAVE_REL_OFFSET(offset);

      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);

      KILL_REL_OFFSET();

      void *start, *end;
      if (target < relocated_ins) {
	start = target;
	int branch_inst_len = xed_decoded_inst_get_length(xptr);
	end = relocated_ins + branch_inst_len; 
	if (inside_protected_range(target)) {
	  add_protected_range(start, end);
	  return true;
	}
      } 
    }
  }
  return false;
}

bool
range_contains_control_flow(void *vstart, void *vend)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t *xptr = &xedd_tmp;
  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);

  char *ins = (char *) vstart;
  char *end = (char *) vend;
  while (ins < end) {

    xed_error_enum_t xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

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

      SAVE_REL_OFFSET(offset);

      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);

      KILL_REL_OFFSET();

      if (target < relocated_ins) {
	// backward jump; if this is a tail call, it should fall on a function
	// entry already in the function entries table
	if (query_function_entry(target)) return true;

	// we may be looking at a bogus jump in data; 
	// we must make sure that its target is in bounds before inspecting it.
	if (!consider_possible_fn_address(target)) return false;
	char *target_addr_in_memory = target - offset;

	xed_decoded_inst_t xtmp;
	xed_decoded_inst_t *xptr = &xtmp;
	xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
	
	xed_error_enum_t xed_error = 
	  xed_decode(xptr, (uint8_t*) target_addr_in_memory, 15);
	if (xed_error != XED_ERROR_NONE) return false;
	
	xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
	const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
	
	switch(xiclass) {
	case XED_ICLASS_PUSH: 
	case XED_ICLASS_PUSHFQ: 
	case XED_ICLASS_PUSHFD: 
	case XED_ICLASS_PUSHF:  
	  {
	    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
	    xed_operand_enum_t   op0_name = xed_operand_name(op0);
	    
	    if (op0_name == XED_OPERAND_REG0) { 
	      xed_reg_enum_t regname = 
		xed_decoded_inst_get_reg(xptr, op0_name);
	      if (x86_isReg_BP(regname)) {
		add_stripped_function_entry(target, 1 /* support */); 
		return true;
	      }
	    }
	  }
	  return false;
	case XED_ICLASS_ADD:
	case XED_ICLASS_SUB:
	  {
	    const xed_operand_t* op0 = xed_inst_operand(xi,0);
	    const xed_operand_t* op1 = xed_inst_operand(xi,1);
	    xed_operand_enum_t   op0_name = xed_operand_name(op0);
	    
	    if ((op0_name == XED_OPERAND_REG0) 
		&& x86_isReg_SP(xed_decoded_inst_get_reg(xptr, op0_name))) {
	      
	      if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
		//-------------------------------------------------
		// adjusting the stack pointer by a constant offset
		//-------------------------------------------------
		int sign = (xiclass == XED_ICLASS_ADD) ? 1 : -1;
		long immedv = sign * 
		  xed_decoded_inst_get_signed_immediate(xptr);
		if (immedv < 0) {
		  add_stripped_function_entry(target, 1 /* support */); 
		  return true;
		}
	      }
	    }
	  }
	  return false;
	default:
	  // not what is expected for the start of a routine.
	  return false;
	}
      } else {
	return range_contains_control_flow(ins, target);
      }
    }
  }
  return false;
}  


static bool
lea_has_zero_offset(xed_decoded_inst_t *xptr)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
  xed_operand_enum_t   op1_name = xed_operand_name(op1);
  if (op1_name == XED_OPERAND_AGEN) { 
    int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
    if (offset == 0) return true;
  } 
  return false;
}

//
// utility to determine the address of the next instruction
//
static char*
xed_next(char* ins)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t* xptr = &xedd_tmp;
  xed_error_enum_t xed_error;
  int offset = 0;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_decoded_inst_zero_keep_mode(xptr);

  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_error == XED_ERROR_NONE) {
    offset = xed_decoded_inst_get_length(xptr);
  }
  return ins + offset;
}

static bool
is_mov_sp_2_bp(char* ins)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t* xptr = &xedd_tmp;
  xed_error_enum_t xed_error;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_decoded_inst_zero_keep_mode(xptr);

  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_error != XED_ERROR_NONE) return false;

  if (xed_decoded_inst_get_iclass(xptr) != XED_ICLASS_MOV)
    return false;

  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
  
  xed_operand_enum_t op0_name = xed_operand_name(op0);
  xed_operand_enum_t op1_name = xed_operand_name(op1);
  
  if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)) { 
    //-------------------------------------------------------------------------
    // register-to-register move 
    //-------------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
    return x86_isReg_BP(reg0) && x86_isReg_SP(reg1);
  }
  return false;
}

static bool
ins_seq_is_std_frame(char* ins)
{
  return is_push_bp(ins) && is_mov_sp_2_bp(xed_next(ins));
}

static const size_t FRAMELESS_PROC_WINDOW = 8;

static bool
ins_seq_has_reg_move_to_bp(char* ins)
{
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t* xptr = &xedd_tmp;
  xed_error_enum_t xed_error;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_decoded_inst_zero_keep_mode(xptr);

  for (size_t i=0; i < FRAMELESS_PROC_WINDOW; i++) {
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
    if (xed_error != XED_ERROR_NONE) {
      ins++;
      continue;
    }
    if (xed_decoded_inst_get_iclass(xptr) == XED_ICLASS_MOV) {
      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
      const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
  
      xed_operand_enum_t op0_name = xed_operand_name(op0);
      xed_operand_enum_t op1_name = xed_operand_name(op1);
  
      if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)) { 
	//-------------------------------------------------------------------------
	// register-to-register move 
	//-------------------------------------------------------------------------
	xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
	if (x86_isReg_BP(reg0))
	  return true;
      }
    }
    ins += xed_decoded_inst_get_length(xptr);
  }
  return false;
}

//
//  Heuristic for identifying common frameless procedure pattern:
//     push bp
//     ...
//     mov SOME_REG, bp
//
//  In other words, a push of bp followed by overwriting bp with some other register
//  indicates a (probable) start of a procedure.
//
static bool
ins_seq_is_common_frameless_proc(char* ins)
{
  return is_push_bp(ins) && ins_seq_has_reg_move_to_bp(xed_next(ins));
}

static bool 
nextins_looks_like_fn_start(char *ins, long offset, xed_decoded_inst_t *xptrin)
{ 
  xed_decoded_inst_t xedd_tmp;
  xed_decoded_inst_t *xptr = &xedd_tmp;
  xed_error_enum_t xed_error;

  ins = ins + xed_decoded_inst_get_length(xptrin);

  if (inside_protected_range(ins + offset)) return false;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);

  for(;;) {
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) return false;

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
    switch(xiclass) {
    case XED_ICLASS_LEA: 
      if (!lea_has_zero_offset(xptr)) return false; 
      // gcc seems to use 0 offset lea as padding between functions
      // treat it as a nop

    case XED_ICLASS_NOP:  case XED_ICLASS_NOP2: case XED_ICLASS_NOP3: 
    case XED_ICLASS_NOP4: case XED_ICLASS_NOP5: case XED_ICLASS_NOP6: 
    case XED_ICLASS_NOP7: case XED_ICLASS_NOP8: case XED_ICLASS_NOP9:
      // nop: move to the next instruction
      ins = ins + xed_decoded_inst_get_length(xptr);
      break;
    case XED_ICLASS_PUSH: 
    case XED_ICLASS_PUSHFQ: 
    case XED_ICLASS_PUSHFD: 
    case XED_ICLASS_PUSHF:  
      return ins_seq_is_std_frame(ins) || ins_seq_is_common_frameless_proc(ins);
      break;

    case XED_ICLASS_ADD:
    case XED_ICLASS_SUB:
      {
	const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
	const xed_operand_t* op0 = xed_inst_operand(xi,0);
	const xed_operand_t* op1 = xed_inst_operand(xi,1);
	xed_operand_enum_t   op0_name = xed_operand_name(op0);

	if ((op0_name == XED_OPERAND_REG0) &&
	    (x86_isReg_SP(xed_decoded_inst_get_reg(xptr, op0_name)))) {

	  if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
	    //------------------------------------------------------------------
	    // we are adjusting the stack pointer by a constant offset
	    //------------------------------------------------------------------
	    int sign = (xiclass == XED_ICLASS_ADD) ? 1 : -1;
	    long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
	    if (immedv < 0) {
	      add_stripped_function_entry(ins + offset, 1 /* support */); 
	      return true;
	    }
	  }
	}
      }
      return false;
    default:
      // not a nop, or what is expected for the start of a routine.
      return false;
      break;
    }
  } 

  return false;
}  


static void 
process_branch(char *ins, long offset, xed_decoded_inst_t *xptr, char* vstart, char* vend)
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

      SAVE_REL_OFFSET(offset);

      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);

      KILL_REL_OFFSET();

      //
      // if branch target is not within bounds, then this branch instruction is bogus ....
      //
      if (! (((vstart + offset) <= target) && (target <= (vend + offset)))) {
	return;
      }
      void *start, *end;
      if (target < relocated_ins) {
        unsigned char *tloc = (unsigned char *) target - offset;
        for (; is_padding(*(tloc-1)); tloc--) { 
	  // extend branch range to before padding
	  target--;
	}

	start = target;
	// protect to end of branch instruction
	int branch_inst_len = xed_decoded_inst_get_length(xptr);
	end = relocated_ins + branch_inst_len; 
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
      if (x86_isReg_BP(basereg))  {
	return 1;
      } else if (x86_isReg_SP(basereg)) {
         int64_t offset = 
	   xed_decoded_inst_get_memory_displacement(xptr, oindex);
         if (offset > 0) {
	   return 1;
	 }
      } else if (x86_isReg_AX(basereg)) {
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
from_ax_reg(xed_decoded_inst_t *xptr)
{
  static xed_operand_enum_t regops[] = { XED_OPERAND_REG0, XED_OPERAND_REG1 };
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  int noperands = xed_decoded_inst_noperands(xptr);

  if (noperands > 2) noperands = 2; // we don't care about more than two operands
  for (int opid = 0; opid < noperands; opid++) { 
    const xed_operand_t *op =  xed_inst_operand(xi, opid);
    xed_operand_enum_t   op_name = xed_operand_name(op);
    if (op_name == regops[opid]) {  
      xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op_name);
      if ((regname == XED_REG_RAX) || (regname == XED_REG_EAX) || (regname == XED_REG_AX)) {
	// operand may perform a read of rax/eax/ax
	switch(xed_operand_rw(op)) {
	case XED_OPERAND_ACTION_R:   // read
	  // case XED_OPERAND_ACTION_RW:  // read and written: skip this case - xor %eax, %eax is OK
	case XED_OPERAND_ACTION_RCW: // read and conditionlly written 
	case XED_OPERAND_ACTION_CR:  // conditionally read
	case XED_OPERAND_ACTION_CRW: // conditionlly read, always written
	  return true;
	default:
	  return false;
	}
      }
    }
  }
  return false;
}


static bool
is_null(unsigned char *ins, int n)
{
	unsigned char result = 0;
	unsigned char *end = ins + n;
	while (ins < end) result |= *ins++;
	if (result == 0) return true;
	else return false;
}

static bool
is_breakpoint(xed_decoded_inst_t *xptr)
{
  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  switch(xiclass) {
  case XED_ICLASS_INT:
  case XED_ICLASS_INT1:
  case XED_ICLASS_INT3: 
    return true;
  default:
    break;
  }
  return false;
}

static bool
invalid_routine_start(unsigned char *ins)
{
  xed_decoded_inst_t xdi;
  xed_decoded_inst_t *xptr = &xdi;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_error_enum_t xed_error = xed_decode(xptr, (uint8_t*) ins, 15);
  
  if (xed_error != XED_ERROR_NONE) return true; 

  if (is_null(ins, xed_decoded_inst_get_length(xptr))) return true;
  if (inst_accesses_callers_mem(xptr)) return true;
  if (from_ax_reg(xptr)) return true;
  if (is_breakpoint(xptr)) return true;

  return false;
}


void x86_dump_ins(void *ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  char inst_buf[1024];

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  if (xed_error == XED_ERROR_NONE) {
    xed_decoded_inst_dump_xed_format(xptr, inst_buf, sizeof(inst_buf), (uint64_t) ins);
    printf("(%p, %d bytes, %s) %s \n" , ins, xed_decoded_inst_get_length(xptr),
	   xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xptr)), inst_buf);
  } else {
    printf("x86_dump_ins: xed decode addr=%p, error = %d\n", ins, xed_error);
  }
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
      x86_isReg_SP(xed_decoded_inst_get_reg(xptr, op0_name))) {

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
	  add_protected_range(prologue_start + ins_offset + 1, 
			      ins + ins_offset + 1);
#ifdef DEBUG_ADDSUB
	  char *end = ins + 1; 
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
    if ((x86_isReg_BP(reg0)) &&	(x86_isReg_SP(reg1))) {
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
    if (x86_isReg_BP(regname)) {
      push_rbp = ins;
      // JMC + MIKE: assume that a push when there is only weak evidence of a fn start
      //     might be a potential function entry
#if 0
      if (fn_start == WEAK_FN){
	add_stripped_function_entry(ins + ins_offset, 1 /* support */);
	fn_start == MODERATE_FN;
      }
#endif
    } else {
      push_other = ins;
      push_other_reg = regname;
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
    if (x86_isReg_BP(regname)) {
      if (push_rbp) {
        add_protected_range(push_rbp + ins_offset + 1, ins + ins_offset + 1);
      } else {
	if (push_other) {
	  if (push_other_reg == regname) {
	    if (push_other) {
	      add_protected_range(push_other + ins_offset + 1, 
				  ins + ins_offset + 1);
	    }
	  } else {
	    // it must match some push. assume the latest.
	    char *push_latest = (push_other > push_rbp) ? push_other : push_rbp;
	    // ... unless we've started to see bad instructions, which makes
	    //     it likely that we're walking through data
	    if (push_latest > last_bad)
	      if (push_latest) {
		add_protected_range(push_latest + ins_offset + 1, 
				    ins + ins_offset + 1);
	      }
	  }
	}
      }
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
  char *save_rbp = (set_rbp > push_rbp) ? set_rbp : push_rbp;
  add_protected_range(save_rbp + ins_offset + 1, ins + ins_offset + 1);
}

