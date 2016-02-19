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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <cstdarg> 
#include <cstring> // for 'memcpy'

//*************************** User Include Files ****************************

#include <include/gnu_dis-asm.h>

#include "x86ISAXed.hpp"

#include <lib/support/diagnostics.h>

extern "C" {
#include <xed-interface.h>
}

//*************************** Global Variables ***************************
static xed_state_t xed_machine_state =
#if defined (HOST_CPU_x86_64)
    { XED_MACHINE_MODE_LONG_64,
      XED_ADDRESS_WIDTH_64b };
#else
      { XED_MACHINE_MODE_LONG_COMPAT_32,
          XED_ADDRESS_WIDTH_32b };
#endif

//*************************** cache decoder ***************************
static MachInsn *mi;
static xed_decoded_inst_t xedd;

static xed_decoded_inst_t*
getDecodeXED(MachInsn *cmi)
{
  if (cmi == mi) {
    return &xedd;
  } 
  else {
    mi = cmi;

    xed_decoded_inst_t *xptr = &xedd;

    xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state);
    xed_decoded_inst_zero_keep_mode(xptr);

    xed_error_enum_t xed_error = xed_decode(xptr, (uint8_t*) mi, 15);
    if (xed_error == XED_ERROR_NONE) {
      return xptr;
    }
    else {
      return NULL;
    }
  }
}

//*************************** x86ISAXed ***************************

x86ISAXed::x86ISAXed(bool is_x86_64)
{
  xed_tables_init(); 
}


x86ISAXed::~x86ISAXed()
{
}


ISA::InsnDesc
x86ISAXed::getInsnDesc(MachInsn* mi, ushort GCC_ATTR_UNUSED opIndex,
                    ushort GCC_ATTR_UNUSED s)
{
  xed_decoded_inst_t *xptr = getDecodeXED(mi);

  ISA::InsnDesc d;

  if (xptr == NULL) {
    return d;
  }

  int offset;
  xed_operand_values_t *vals;
  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

  switch(xiclass) {
   case XED_ICLASS_INVALID:
     d.set(InsnDesc::INVALID);
     break;

      // unconditional jump
   case XED_ICLASS_JMP:      case XED_ICLASS_JMP_FAR:
      vals = xed_decoded_inst_operands(xptr);
      offset = xed_operand_values_get_branch_displacement_int32(vals);
      if (offset != 0) {
         d.set(ISA::InsnDesc::BR_UN_COND_REL);
      } else {
         d.set(ISA::InsnDesc::BR_UN_COND_IND);
      }
      break;

      // return
    case XED_ICLASS_RET_FAR:  case XED_ICLASS_RET_NEAR:
      d.set(ISA::InsnDesc::SUBR_RET);
      break;

      // conditional branch
    case XED_ICLASS_JB:         case XED_ICLASS_JBE:
    case XED_ICLASS_JL:         case XED_ICLASS_JLE:
    case XED_ICLASS_JNB:        case XED_ICLASS_JNBE:
    case XED_ICLASS_JNL:        case XED_ICLASS_JNLE:
    case XED_ICLASS_JNO:        case XED_ICLASS_JNP:
    case XED_ICLASS_JNS:        case XED_ICLASS_JNZ:
    case XED_ICLASS_JO:         case XED_ICLASS_JP:
    case XED_ICLASS_JRCXZ:      case XED_ICLASS_JS:
    case XED_ICLASS_JZ:
      vals = xed_decoded_inst_operands(xptr);
      offset = xed_operand_values_get_branch_displacement_int32(vals);
      if (offset != 0) {
         d.set(ISA::InsnDesc::INT_BR_COND_REL);
      } else {
         d.set(ISA::InsnDesc::INT_BR_COND_IND); // arbitrarily choose int
      }
      break;

    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      vals = xed_decoded_inst_operands(xptr);
      offset = xed_operand_values_get_branch_displacement_int32(vals);
      if (offset != 0) {
         d.set(ISA::InsnDesc::SUBR_REL);
      } else {
         d.set(ISA::InsnDesc::SUBR_IND);
      }
      break;

   default:
      d.set(ISA::InsnDesc::OTHER);
      break;
  }
  return d;
}



ushort
x86ISAXed::getInsnSize(MachInsn* mi)
{
  xed_decoded_inst_t *xptr = getDecodeXED(mi);

  if (xptr == NULL) {
    return 0;
  }
  return (ushort)xed_decoded_inst_get_length(xptr);
}


VMA
x86ISAXed::getInsnTargetVMA(MachInsn* mi, VMA vma, ushort GCC_ATTR_UNUSED opIndex,
                         ushort GCC_ATTR_UNUSED sz)
{
  xed_decoded_inst_t *xptr = getDecodeXED(mi);

  if (xptr != NULL) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);
    int offset = xed_operand_values_get_branch_displacement_int32(vals);

    int len = xed_decoded_inst_get_length(xptr);
    char* insn_end = (char*)mi + len;
    VMA absoluteTarget = (VMA)(insn_end + offset);

    if (absoluteTarget != 0) {
      VMA relativeTarget = GNUvma2vma(absoluteTarget, mi, vma);
      return relativeTarget;
    }
  }
  return 0;
}


//****************************************************************************

