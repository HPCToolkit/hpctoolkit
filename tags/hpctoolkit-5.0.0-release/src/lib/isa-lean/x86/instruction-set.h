// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2011, Rice University
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

#ifndef isa_lean_x86_instruction_set_h
#define isa_lean_x86_instruction_set_h

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

# include "xed-interface.h"

#ifdef __cplusplus
};
#endif

//***************************************************************************
// 
//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

static inline bool
x86_isReg_BP(xed_reg_enum_t reg)
{
  return (
#if defined (HOST_CPU_x86_64)
	  reg == XED_REG_RBP ||
#endif
	  reg == XED_REG_EBP ||
	  reg == XED_REG_BP);
}


static inline bool
x86_isReg_SP(xed_reg_enum_t reg)
{
  return (
#if defined (HOST_CPU_x86_64)
	  reg == XED_REG_RSP ||
#endif
	  reg == XED_REG_ESP ||
	  reg == XED_REG_SP);
}

static inline bool
x86_isReg_IP(xed_reg_enum_t reg)
{
  return (
#if defined (HOST_CPU_x86_64)
	  reg == XED_REG_RIP ||
#endif
	  reg == XED_REG_EIP ||
	  reg == XED_REG_IP);
}


static inline bool
x86_isReg_AX(xed_reg_enum_t reg)
{
  return (
#if defined (HOST_CPU_x86_64)
	  reg == XED_REG_RAX ||
#endif
	  reg == XED_REG_EAX ||
	  reg == XED_REG_AX);
}



#ifdef __cplusplus
};
#endif



#endif // isa_lean_x86_instruction_set_h
