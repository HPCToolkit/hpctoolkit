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
// Copyright ((c)) 2002-2019, Rice University
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

//************************* System Include Files ****************************

#include <stdbool.h>

//************************** XED Include Files ******************************

#ifdef __cplusplus
extern "C" {
#endif

# include <xed-interface.h>

#ifdef __cplusplus
};
#endif

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// Laks: macro for detecting registers
//
#if defined (HOST_CPU_x86_64)		  
#define X86_ISREG(REG) \
static inline bool \
x86_isReg_ ## REG  (xed_reg_enum_t reg) \
{ 					\
  return (				\
	  reg == XED_REG_R ## REG  ||   \
	  reg == XED_REG_E ## REG  ||   \
	  reg == XED_REG_ ## REG  );	\
} 
#else
#define X86_ISREG(REG) \
static inline bool \
x86_isReg_ ## REG  (xed_reg_enum_t reg) \
{ 					\
  return (			        \
	  reg == XED_REG_E ## REG  ||   \
	  reg == XED_REG_ ## REG  );	\
} 
#endif

#define X86_ISREG_R(REG) 	 	\
static inline bool 		 	\
x86_isReg_R ## REG (xed_reg_enum_t reg) \
{				 	\
  return (reg == XED_REG_R ## REG ); 	\
}

//***************************************************************************
// 
//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

X86_ISREG(BP)

X86_ISREG(SP)

X86_ISREG(IP)

X86_ISREG(AX)

X86_ISREG(BX)

X86_ISREG(CX)

X86_ISREG(DX)

X86_ISREG(SI)

X86_ISREG(DI)

X86_ISREG_R(8)

X86_ISREG_R(9)

X86_ISREG_R(10)

X86_ISREG_R(11)

X86_ISREG_R(12)

X86_ISREG_R(13)

X86_ISREG_R(14)

X86_ISREG_R(15)


//***************************************************************************

#ifdef __cplusplus
};
#endif


#endif // isa_lean_x86_instruction_set_h
