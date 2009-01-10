// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef INTERVALS_H
#define INTERVALS_H

//************************* System Include Files ****************************

#include <stdbool.h>

//*************************** User Include Files ****************************

#include "splay-interval.h"

//*************************** Forward Declarations **************************

//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

//***************************************************************************
// 
//***************************************************************************

typedef enum {
  FrmTy_NULL = 0,
  FrmTy_SP,   // Parent's SP/BP are SP-based; RA is SP-based (or reg)
  FrmTy_BP,   // Parent's SP/BP are BP-based; RA is BP-based (or reg)

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

//***************************************************************************
// 
//***************************************************************************

struct unw_interval_s {
  struct splay_interval_s common; // common splay tree fields

  framety_t  ty   : 16;
  frameflg_t flgs : 16;

  int sp_pos; // parent's SP position
  int bp_pos; // parent's BP position
  int ra_arg; // RA position or register
};

typedef struct unw_interval_s unw_interval_t;


interval_status build_intervals(char* ins, unsigned int len);

unw_interval_t* new_ui(char* start_addr, 
		       framety_t ty, frameflg_t flgs, 
		       int sp_pos, int bp_pos, int ra_arg,
		       unw_interval_t* prev);

static inline bool 
ui_cmp(unw_interval_t* x, unw_interval_t* y)
{
  return ((x->ty == y->ty) && 
	  (x->flgs == y->flgs) &&
	  (x->sp_pos == y->sp_pos) &&
	  (x->bp_pos == y->bp_pos) &&
	  (x->ra_arg == y->ra_arg));
}

void dump_ui(unw_interval_t* u, int dump_to_stdout);

long ui_count();                    // FIXME:interface
long suspicious_count();            // FIXME:interface
void suspicious_interval(void *pc); // FIXME:interface
void link_ui(unw_interval_t* current, unw_interval_t* next); // FIXME: INTERFACE

#ifdef __cplusplus
};
#endif


//***************************************************************************

#endif // INTERVALS_H
