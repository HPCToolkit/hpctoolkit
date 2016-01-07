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
// HPCToolkit's MIPS Unwinder
// 
// Nathan Tallent
// Rice University
//
// When part of HPCToolkit, this code assumes HPCToolkit's license;
// see www.hpctoolkit.org.
//
//***************************************************************************

#ifndef mips_unwind_interval_h
#define mips_unwind_interval_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include "mips-unwind-cfg.h"

//*************************** Forward Declarations **************************

//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

//***************************************************************************
// framety_t and frameflg_t
//***************************************************************************

typedef enum {
  FrmTy_NULL = 0,
  FrmTy_SP,   // Parent's SP/FP are SP-based; RA is SP-based or reg
  FrmTy_FP,   // Parent's SP/FP are FP-based; RA is FP-based or reg

} framety_t;


typedef enum {
  FrmFlg_NULL      = 0x0,
  FrmFlg_RAReg     = 0x1, // RA is in a register (otherwise, in the frame)
  FrmFlg_FrmSzUnk  = 0x2, // frame size is unknown (e.g., alloca)
  FrmFlg_FPOfstPos = 0x4, // offsets from FP are positive (not negative)
  FrmFlg_FPInV     = 0x8, // (parent) FP has been moved to register v0/v1

} frameflg_t;


static inline bool 
frameflg_isset(int16_t flags, frameflg_t flg)
{
  return (flags & flg);
}


static inline void 
frameflg_set(int16_t* flags, frameflg_t flg)
{
  *flags = (*flags | flg);
}


static inline void 
frameflg_unset(int16_t* flags, frameflg_t flg)
{
  *flags = (*flags & ~flg);
}


const char* 
framety_string(framety_t ty);


//***************************************************************************
// unw_interval_t
//***************************************************************************

// an invalid position (note that in particular, 0 may be a valid postion)
#define unwarg_NULL (-1)

typedef struct {
  HPC_IFNO_UNW_LITE(splay_interval_t common;) // common splay tree fields

  //--------------------------------------------------
  // frame type and flags
  //--------------------------------------------------
  framety_t              ty    : 16;
  int16_t /*frameflg_t*/ flgs;

  //--------------------------------------------------
  // SP, FP, RA arguments, all initialized to unwarg_NULL
  //--------------------------------------------------

  // FrmTy_SP:           distance of parent's SP relative to current SP
  // FrmTy_FP: 
  // - FrmFlg_FPOfstPos: distance of parent's SP relative to current FP
  // - otherwise:        offset of parent's SP within frame, relative to FP
  int sp_arg;

  // *: offset of parent's FP within frame (relative to SP/FP)
  int fp_arg;

  // *, FrmFlg_RAReg: register name for RA
  // *, otherwise   : offset of RA within frame (relative to SP/FP)
  int ra_arg;

} unw_interval_t;


unw_interval_t* 
new_ui(char* start_addr, framety_t ty, int flgs, 
       int sp_arg, int fp_arg, int ra_arg, unw_interval_t* prev);

static inline bool 
ui_cmp(unw_interval_t* x, unw_interval_t* y)
{
  return ((x->ty == y->ty) && 
	  (x->flgs == y->flgs) &&
	  (x->sp_arg == y->sp_arg) &&
	  (x->fp_arg == y->fp_arg) &&
	  (x->ra_arg == y->ra_arg));
}

void 
ui_dump(unw_interval_t* u);

// FIXME: these should be part of the common interface
void suspicious_interval(void *pc);
void ui_link(unw_interval_t* current, unw_interval_t* next);

//***************************************************************************

// Special support for using unw_interval_t's as either pointers or
// objects.  This allows seamless transition between dynamic and
// static (HPC_UNW_LITE) allocation.
//
//   UNW_INTERVAL_t: the type of an unw_interval_t
//
//   NEW_UI: return a new unw_interval_t (statically or dynamically allocated)
//
//   UNW_INTERVAL_NULL: the appropriate value for a NULL unw_interval_t
//
//   UI_IS_NULL: test whether the given interval is NULL
//
//   UI_FLD: extract a field from an UNW_INTERVAL_t
//
//   UI_ARG: pass an UNW_INTERVAL_t as an argument
//
//   CASTTO_UNW_CURSOR_INTERVAL_t: cast to an UNW_CURSOR_INTERVAL_t
//
//   CASTTO_UNW_INTERVAL_t: cast to an UNW_INTERVAL_t
//
//   UI_CMP_OPT: "optimized" ui_cmp (a pointer comparison if
//               UNW_INTERVAL_t is a pointer)

#if (HPC_UNW_LITE)

//-----------------------------------------------------------
// both UNW_CURSOR_INTERVAL_t and UNW_INTERVAL_t are structs
//-----------------------------------------------------------

#  define UNW_INTERVAL_t unw_interval_t

#  define NEW_UI(addr, x_ty, x_flgs, x_sp_arg, x_fp_arg, x_ra_arg, prev) \
     (unw_interval_t){.ty = x_ty, .flgs = (int16_t)x_flgs,               \
                      .sp_arg = x_sp_arg, .fp_arg = x_fp_arg,            \
                      .ra_arg = x_ra_arg }

#  define UNW_INTERVAL_NULL             \
     NEW_UI(0, FrmTy_NULL, FrmFlg_NULL, \
            unwarg_NULL, unwarg_NULL, unwarg_NULL, NULL)

#  define UI_IS_NULL(ui) ((ui).ty == FrmTy_NULL)

#  define UI_FLD(ui, field) (ui).field

#  define UI_ARG(ui) &(ui)

#  define CASTTO_UNW_CURSOR_INTERVAL_t(x) (*((UNW_CURSOR_INTERVAL_t*)&(x)))
#  define CASTTO_UNW_INTERVAL_t(x)        (*((UNW_INTERVAL_t*)&(x)))

#  define UI_CMP_OPT(x_ui, y_ui) ui_cmp(&(x_ui), &(y_ui))

#else

//-----------------------------------------------------------
// both UNW_CURSOR_INTERVAL_t and UNW_INTERVAL_t are pointers
//-----------------------------------------------------------

#  define UNW_INTERVAL_t unw_interval_t*

#  define NEW_UI new_ui

#  define UNW_INTERVAL_NULL NULL

#  define UI_IS_NULL(ui) (ui == NULL)

#  define UI_FLD(ui, field) (ui)->field

#  define UI_ARG(ui) ui

#  define CASTTO_UNW_CURSOR_INTERVAL_t(x) (UNW_CURSOR_INTERVAL_t)(x)
#  define CASTTO_UNW_INTERVAL_t(x)        (UNW_INTERVAL_t)(x)

#  define UI_CMP_OPT(x_ui, y_ui) ((x_ui) == (y_ui))

#endif


//***************************************************************************
// external interface
//***************************************************************************

// given a PC, return the interval for it
UNW_INTERVAL_t
demand_interval(void* pc, bool isTopFrame);


#if (!HPC_UNW_LITE)
interval_status 
build_intervals(char* ins, unsigned int len);
#endif


//***************************************************************************

#ifdef __cplusplus
};
#endif

//***************************************************************************

#endif // mips_unwind_interval_h
