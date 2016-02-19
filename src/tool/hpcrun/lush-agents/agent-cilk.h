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
// File: 
//   $HeadURL$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_agents_agent_cilk_h
#define lush_agents_agent_cilk_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>
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
    // NOTE: coordinate with lush_lip_getLMId() and lush_lip_getLMIP()
    uint64_t lm_id;
    uint64_t lm_ip;
  } u;
};


static inline void 
cilk_ip_set(cilk_ip_t* x, ip_normalized_t ip /*uint32_t status*/)
{
  lush_lip_setLMId(&(x->official_lip), (uint64_t)ip.lm_id);
  lush_lip_setLMIP(&(x->official_lip), (uint64_t)ip.lm_ip);
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
  UnwTy_NULL    = 0,
  UnwTy_Master,  // master (non-worker thread)

  UnwTy_WorkerLcl, // worker: user context is fully on local stack
  UnwTy_Worker     // worker: user context is partially on cactus stack
};

static inline bool 
unw_ty_is_master(unw_ty_t ty) 
{
  return (ty == UnwTy_Master);
}

static inline bool 
unw_ty_is_worker(unw_ty_t ty) 
{
  return ((ty == UnwTy_WorkerLcl) || (ty == UnwTy_Worker));
}


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef enum unw_seg_e unw_seg_t;

enum unw_seg_e {
  // Segments of a Cilk physical stack
  // NOTE: ordering is important (inner to outer contexts)
  UnwSeg_NULL   = 0,
  UnwSeg_CilkRT,    // Cilk runtime support
  UnwSeg_User,      // User code
  UnwSeg_CilkSched  // Cilk scheduler
};


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

typedef enum unw_flg_e unw_flg_t;

enum unw_flg_e {
  UnwFlg_NULL       = 0x00,
  UnwFlg_SeenUser   = 0x01, // user code has been seen (inter)
  UnwFlg_HaveLCtxt  = 0x02, // logical context has been obtained (inter)
  UnwFlg_BegLNote   = 0x04, // past beginning note of an lchord
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
    void* ref_ip;                // reference physical ip (unnormalized)
    ip_normalized_t ref_ip_norm; // reference physical ip (normalized)

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
