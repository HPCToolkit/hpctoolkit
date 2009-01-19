// -*-Mode: C++;-*- // technically C99
// $Id$

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
  FrmFlg_NULL    = 0x0,
  FrmFlg_RAReg   = 0x1, // RA is in a register (otherwise, in the frame)

} frameflg_t;


static inline bool 
frameflg_isset(int flags, frameflg_t flg)
{
  return (flags & flg);
}


#if 0
static inline void 
frameflg_set(int* flags, flagsflg_t flg)
{
  *flags = (*flags | flg);
}


static inline void 
frameflg_unset(int* flags, flagsflg_t flg)
{
  *flags = (*flags & ~flg);
}
#endif


const char* 
framety_string(framety_t ty);


//***************************************************************************
// unw_interval_t
//***************************************************************************

// an invalid position (note that in particular, 0 may be a valid postion)
#define unwpos_NULL (-1)

typedef struct {
  HPC_IFNO_UNW_LITE(splay_interval_t common;) // common splay tree fields

  framety_t  ty   : 16;
  frameflg_t flgs : 16;

  int sp_pos; // parent's SP position; init to unwpos_NULL
  int fp_pos; // parent's FP position; init to unwpos_NULL
  int ra_arg; // RA position or register; init to unwpos_NULL

} unw_interval_t;


unw_interval_t* 
new_ui(char* start_addr, framety_t ty, frameflg_t flgs, 
       int sp_pos, int fp_pos, int ra_arg, unw_interval_t* prev);

static inline bool 
ui_cmp(unw_interval_t* x, unw_interval_t* y)
{
  return ((x->ty == y->ty) && 
	  (x->flgs == y->flgs) &&
	  (x->sp_pos == y->sp_pos) &&
	  (x->fp_pos == y->fp_pos) &&
	  (x->ra_arg == y->ra_arg));
}

void 
ui_dump(unw_interval_t* u, int dump_to_stdout);

// FIXME: these should be part of the common interface
long ui_count();
long suspicious_count();
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

#  define NEW_UI(addr, x_ty, x_flgs, x_sp_pos, x_fp_pos, x_ra_arg, prev) \
     (unw_interval_t){.ty = x_ty, .flgs = x_flgs,                        \
                      .sp_pos = x_sp_pos, .fp_pos = x_fp_pos,            \
                      .ra_arg = x_ra_arg }

#  define UNW_INTERVAL_NULL             \
     NEW_UI(0, FrmTy_NULL, FrmFlg_NULL, \
            unwpos_NULL, unwpos_NULL, unwpos_NULL, NULL)

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
demand_interval(void* pc);


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
