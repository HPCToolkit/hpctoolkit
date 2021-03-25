// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <gtpin.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/sample-sources/libdl.h>



//******************************************************************************
// macros
//******************************************************************************

#define str(t) #t
#define xstr(t) str(t)
#define gtpin_path() xstr(GTPIN_LIBDIR) "/libgtpin.so"

#define GTPIN_FN_NAME(f) DYN_FN_NAME(f)

#define GTPIN_FN(type, fn, args)			\
  static type (*GTPIN_FN_NAME(fn)) args

#define HPCRUN_GTPIN_CALL(fn, args) (GTPIN_FN_NAME(fn) args)

#define FORALL_GTPIN_ROUTINES(macro) \
  macro(KNOB_FindArg)		     \
  macro(KNOB_AddValue)		     \
  				     \
  macro(GTPin_BBLHead)		     \
  macro(GTPin_BBLNext)		     \
  macro(GTPin_BBLValid)		     \
  				     \
  macro(GTPin_InsHead)		     \
  macro(GTPin_InsTail)		     \
  macro(GTPin_InsValid)		     \
  macro(GTPin_InsOffset)	     \
  macro(GTPin_InsNext)		     \
  macro(GTPin_InsPrev)         \
  macro(GTPin_InsDisasm)       \
  macro(GTPin_InsGetExecSize)   \
  macro(GTPin_InsIsFlagModifier)    \
  macro(GTPin_InsGED)   \
  macro(GTPin_InsIsChangingIP)     \
  macro(GTPin_InsIsEOT) \
  				     \
  macro(GTPin_OnKernelBuild)	     \
  macro(GTPin_OnKernelRun)	     \
  macro(GTPin_OnKernelComplete)	     \
  macro(GTPIN_Start)		     \
				     \
  macro(GTPin_KernelExec_GetKernel)  \
				     \
  macro(GTPin_KernelProfilingActive) \
  macro(GTPin_KernelGetName)	     \
  macro(GTPin_KernelGetSIMD)    \
  				     \
  macro(GTPin_OpcodeprofInstrument)  \
  macro(GTPin_LatencyInstrumentPre)   \
  macro(GTPin_LatencyInstrumentPost_Mem)    \
  macro(GTPin_SimdProfInstrument)   \
  macro(GTPin_InsGetExecMask)   \
  macro(GTPin_InsGetPredArgs)   \
  macro(GTPin_InsIsMaskEnabled)   \
  macro(GTPin_LatencyAvailableRegInstrument)    \
  				     \
  macro(GTPin_GetElf)		     \
  				     \
  macro(GTPin_MemSampleLength)	     \
  macro(GTPin_MemClaim)		     \
  macro(GTPin_MemRead)




//******************************************************************************
// local data
//******************************************************************************

GTPIN_FN
(
 GTPinKnob, 
 KNOB_FindArg,
 (
  const char *name
 )
);


GTPIN_FN
(
 KNOB_STATUS,
 KNOB_AddValue, 
 (
  GTPinKnob knob, 
  KnobValue *knob_value
 )
);


GTPIN_FN
(
 GTPinBBL, 
 GTPin_BBLHead,
 (
  GTPinKernel kernel
 )
);


GTPIN_FN
(
 void,
 GTPIN_Start,
 (
  void
 )
);


GTPIN_FN
(
 GTPinBBL, 
 GTPin_BBLNext,
 (
  GTPinBBL block 
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_BBLValid,
 (
  GTPinBBL block 
 )
);


GTPIN_FN
(
 GTPinINS, 
 GTPin_InsHead,
 (
  GTPinBBL block 
 )
);


GTPIN_FN
(
 GTPinINS, 
 GTPin_InsTail,
 (
  GTPinBBL block 
 )
);


GTPIN_FN
(
 int32_t, 
 GTPin_InsOffset,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 GTPinINS, 
 GTPin_InsNext,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 GTPinINS, 
 GTPin_InsPrev,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_InsDisasm,
 (
  ged_ins_t* ged_ins,
  uint32_t buf_size,
  char* buf,
  uint32_t* str_size
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsGetExecSize,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsIsFlagModifier,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 ged_ins_t, 
 GTPin_InsGED,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsIsChangingIP,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsIsEOT,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsValid,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 void,
 GTPin_OnKernelBuild, 
 (
  KernelBuildCallBackFunc fn,
  void *v
 )
);


GTPIN_FN
(
 void,
 GTPin_OnKernelRun, 
 (
  KernelRunCallBackFunc fn,
  void *v
 )
);


GTPIN_FN
(
 void,
 GTPin_OnKernelComplete,
 (
  KernelCompleteCallBackFunc fn,
  void *v
 )
);


GTPIN_FN
(
 GTPinKernel,
 GTPin_KernelExec_GetKernel,
 (
  GTPinKernelExec kernelExec
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_KernelProfilingActive,
 (
  GTPinKernelExec kernelExec, 
  uint32_t activate
 )
);


GTPIN_FN
(
 uint32_t,
 GTPin_MemSampleLength,
 (
  GTPinMem mem
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_KernelGetName,
 (
  GTPinKernel kernel, 
  uint32_t buf_size, 
  char *buf, 
  uint32_t *str_size
 )
);


GTPIN_FN
(
 GTPINTOOL_SIMD, 
 GTPin_KernelGetSIMD,
 (
  GTPinKernel kernel
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_GetElf,
 (
  GTPinKernel kernel, 
  uint32_t buf_size, 
  char *buf,
  uint32_t *elf_size
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_MemClaim,
 (
  GTPinKernel kernel, 
  uint32_t bytes,
  GTPinMem *mem
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_OpcodeprofInstrument,
 (
  GTPinINS ins, 
  GTPinMem countSlot
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_LatencyInstrumentPre,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_LatencyInstrumentPost_Mem,
 (
  GTPinINS ins,
  GTPinMem mem,
  uint32_t useReg
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_SimdProfInstrument,
 (
  GTPinINS ins,
  bool maskCtrl,
  uint32_t execMask,
  const GenPredArgs* predArgs,
  GTPinMem countSlot
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_InsGetExecMask,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 GenPredArgs, 
 GTPin_InsGetPredArgs,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 bool, 
 GTPin_InsIsMaskEnabled,
 (
  GTPinINS ins
 )
);


GTPIN_FN
(
 uint32_t, 
 GTPin_LatencyAvailableRegInstrument,
 (
  GTPinKernel kernel
 )
);


GTPIN_FN
(
 GTPINTOOL_STATUS, 
 GTPin_MemRead,
 (
  GTPinMem mem,
  uint32_t mem_block_id, 
  uint32_t buf_size, 
  char* buf,
  uint32_t *mem_size
 )
);



//******************************************************************************
// private operations
//******************************************************************************

static int
gtpin_bind
(
 void
)
{
#ifndef HPCRUN_STATIC_LINK  
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(gtpin, gtpin_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);
  
#define GTPIN_BIND(fn)        \
  CHK_DLSYM(gtpin, fn);
  
  FORALL_GTPIN_ROUTINES(GTPIN_BIND)
    
#undef GTPIN_BIND
    
  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK  
}

