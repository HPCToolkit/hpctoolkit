// -*-Mode: C++;-*- // technically C99
// $Id: agent-cilk.h 1467 2008-06-20 03:14:14Z eraxxon $

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_agents_agent_cilk_h
#define lush_agents_agent_cilk_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

//*************************** Cilk Include Files ****************************

#include <cilk/cilk.h>          /* Cilk (installed) */
#include <cilk/cilk-internal.h> /* Cilk (not installed) */

//*************************** User Include Files ****************************

#include <lush/lushi.h>
#include <lush/lushi-cb.h>

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//*************************** Forward Declarations **************************


// **************************************************************************
// 
// **************************************************************************

typedef union cilk_ip cilk_ip_t;

union cilk_ip {
  // ------------------------------------------------------------
  // LUSH type
  // ------------------------------------------------------------
  lush_lip_t official_lip;
  
  // ------------------------------------------------------------
  // superimposed with:
  // ------------------------------------------------------------
  struct {
    void* ip;
    //uint32_t status;
  } u;
};

static inline void cilk_ip_init(cilk_ip_t* x, void* ip /*uint32_t status*/)
{
  x->u.ip = ip;
  //x->u.status = status;
}


// **************************************************************************
// 
// **************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef enum unw_ty_e  unw_ty_t;

enum unw_ty_e {
  UNW_TY_NULL    = 0,
  UNW_TY_MASTER,  // master (non-worker thread)

  UNW_TY_WORKER0, // worker 0 before it steals
  UNW_TY_WORKER1  // worker 0 after it steals; all other workers
};

static inline bool unw_ty_is_master(unw_ty_t ty) 
{
  return (ty == UNW_TY_MASTER);
}

static inline bool unw_ty_is_worker(unw_ty_t ty) 
{
  return ((ty == UNW_TY_WORKER0) || (ty == UNW_TY_WORKER1));
}


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef enum unw_seg_e unw_seg_t;

enum unw_seg_e {
  // Segments of a Cilk physical stack
  // NOTE: ordering is important (inner to outer contexts)
  UNW_SEG_NULL   = 0,
  UNW_SEG_CILKRT,    // Cilk runtime support
  UNW_SEG_USER,      // User code
  UNW_SEG_CILKSCHED  // Cilk scheduler
};


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef enum unw_flg_e unw_flg_t;

enum unw_flg_e {
  UNW_FLG_NULL       = 0x00,
  UNW_FLG_SEEN_USER  = 0x01, // user code has been seen (inter)
  UNW_FLG_HAVE_LCTXT = 0x02, // logical context has been obtained (inter)
  UNW_FLG_BEG_LNOTE  = 0x04, // past beginning note an an lchord
};


// **************************************************************************

typedef union cilk_cursor cilk_cursor_t;

union cilk_cursor {
  // ------------------------------------------------------------
  // LUSH type
  // ------------------------------------------------------------
  lush_lcursor_t official_cursor;

  // ------------------------------------------------------------
  // superimposed with:
  // ------------------------------------------------------------
  struct {
    // ---------------------------------
    // inter-bichord data (valid for the whole of one logical unwind)
    // ---------------------------------
    unw_ty_t  ty      : 8; // type of current unwind
    unw_seg_t prev_seg: 8; // ty of prev bi-chord
    unw_flg_t flg     : 8; // unwind flags
    unsigned  xxx     : 8; // UNUSED
    CilkWorkerState* cilk_worker_state;
    Closure*         cilk_closure; // modified during traversal
    
    // ---------------------------------
    // intra-bichord data (valid for only one bichord)
    // ---------------------------------
    void* ref_ip;          // reference physical ip

  } u;
};


static inline bool
csr_is_flag(cilk_cursor_t* csr, unw_flg_t flg)
{
  return (csr->u.flg & flg);
}

static inline void
csr_set_flag(cilk_cursor_t* csr, unw_flg_t flg)
{
  csr->u.flg = (csr->u.flg | flg);
}

static inline void
csr_unset_flag(cilk_cursor_t* csr, unw_flg_t flg)
{
  csr->u.flg = (csr->u.flg & ~flg);
}


// NOTE: Local work (innermost) is pushed and popped from the BOTTOM
//   or the TAIL of the deque while thieves steal from the TOP or HEAD
//   (outermost).
static inline Closure* 
CILKWS_CL_DEQ_TOP(CilkWorkerState* x)
{
  return x->context->Cilk_RO_params->deques[x->self].top; // outermost!
}

static inline Closure* 
CILKWS_CL_DEQ_BOT(CilkWorkerState* x)
{
  return x->context->Cilk_RO_params->deques[x->self].bottom; // innermost!
}

static inline volatile CilkStackFrame** 
CILKWS_FRAME_DEQ_HEAD(CilkWorkerState* x)
{
  return x->cache.head; // outermost!
}

static inline volatile CilkStackFrame** 
CILKWS_FRAME_DEQ_TAIL(CilkWorkerState* x)
{
  return x->cache.tail; // innermost!
}

static inline void* /* void (*) () */
CILKFRM_PROC(CilkStackFrame* x) {
  return x->sig[0].inlet;
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_agents_agent_cilk_h */
