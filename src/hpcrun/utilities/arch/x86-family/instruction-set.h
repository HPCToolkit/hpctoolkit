// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef isa_lean_x86_instruction_set_h
#define isa_lean_x86_instruction_set_h

//************************* System Include Files ****************************

#include <stdbool.h>

//************************** XED Include Files ******************************

#ifdef __cplusplus
extern "C" {
#endif

#if __has_include(<xed/xed-interface.h>)
# include <xed/xed-interface.h>
#else
# include <xed-interface.h>
#endif

#ifdef __cplusplus
};
#endif

//*************************** User Include Files ****************************


//*************************** Forward Declarations **************************

//***************************************************************************
// Laks: macro for detecting registers
//
#if defined (HOST_CPU_x86_64)
#define X86_ISREG(REG) \
static inline bool \
x86_isReg_ ## REG  (xed_reg_enum_t reg) \
{                                       \
  return (                              \
          reg == XED_REG_R ## REG  ||   \
          reg == XED_REG_E ## REG  ||   \
          reg == XED_REG_ ## REG  );    \
}
#else
#define X86_ISREG(REG) \
static inline bool \
x86_isReg_ ## REG  (xed_reg_enum_t reg) \
{                                       \
  return (                              \
          reg == XED_REG_E ## REG  ||   \
          reg == XED_REG_ ## REG  );    \
}
#endif

#define X86_ISREG_R(REG)                \
static inline bool                      \
x86_isReg_R ## REG (xed_reg_enum_t reg) \
{                                       \
  return (reg == XED_REG_R ## REG );    \
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
