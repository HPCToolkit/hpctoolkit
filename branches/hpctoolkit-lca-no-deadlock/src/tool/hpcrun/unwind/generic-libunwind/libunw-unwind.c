//
// This software was produced with support in part from the Defense Advanced
// Research Projects Agency (DARPA) through AFRL Contract FA8650-09-C-1915. 
// Nothing in this work should be construed as reflecting the official policy or
// position of the Defense Department, the United States government, or
// Rice University.
//
//***************************************************************************
//
// File:
//   libunw-unwind.c
//
// Purpose:
//    Implement the hpcrun unwind primitives using libunwind
//
//        hpcrun_unw_init
//        hpcrun_unw_init_cursor
//        hpcrun_unw_get_ra_loc
//        hpcrun_unw_get_ip_reg
//        hpcrun_unw_step
//        hpcrun_unw_get_ip_unnorm_reg
//
//***************************************************************************

//************************************************
// system includes
//************************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <ucontext.h>

#include <include/hpctoolkit-config.h>

//************************************************
// external includes
//************************************************

#include <monitor.h>
#include <libunwind.h>

//************************************************
// local includes
//************************************************

#include <messages/messages.h>

#include <unwind/common/unw-datatypes.h>
#include <unwind/common/unwind.h>

#include <utilities/arch/context-pc.h>

//************************************************
// local utility functions and definitions
//************************************************

static inline size_t
pabs(ptrdiff_t diff)
{
  return diff < 0 ? -diff : diff;
}


//************************************************
// interface functions
//************************************************

// ----------------------------------------------------------
// hpcrun_unw_init
// ----------------------------------------------------------

extern void hpcrun_set_real_siglongjmp(void);

void
hpcrun_unw_init(void)
{
  hpcrun_set_real_siglongjmp();
}


// ----------------------------------------------------------
// hpcrun_unw_get_ip_reg
// ----------------------------------------------------------

int
hpcrun_unw_get_ip_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  unw_word_t tmp;
  int rv = unw_get_reg(&(cursor->uc), UNW_REG_IP, &tmp);
  *val = (void*) tmp;
  return rv;
}

int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  *val = cursor->pc_unnorm;
  return 0;
}

int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* cursor, ip_normalized_t* val)
{
  *val = cursor->pc_norm;
  return 0;
}

// ----------------------------------------------------------
// hpcrun_unw_get_ra_loc (no trampolines yet)
// ----------------------------------------------------------

void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* c)
{
  return NULL;
}

// ----------------------------------------------------------
// hpcrun_unw_init_cursor
// ----------------------------------------------------------

static unsigned UNW_LIMIT = 10;   // empirically determined
static unsigned THRESHOLD = 256;  // empirically determined

bool unw_cursor_trace = false;

void
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* h_cursor, void* context)
{
  unw_context_t unw_c;
  unw_cursor_t* cursor = &(h_cursor->uc);

  void* pc = hpcrun_context_pc(context);

  TMSG(LIBUNW_TRACE, "Init Cursor: context pc = %p", pc);

  unw_word_t ip;
  void* ipp;

  unw_getcontext(&unw_c);
  
  unw_init_local(cursor, &unw_c);

  unw_get_reg(cursor, UNW_REG_IP, &ip);
  ipp = (void*) ip;

  TMSG(LIBUNW_TRACE, "Init Cursor: cursor ip = %p", ipp);

  // unwind some max # of steps to get back to the pc from the context
  for (int i=0; i < UNW_LIMIT; i++) {
    if (pabs(ipp - pc) < THRESHOLD) break;
    unw_step(cursor);
    unw_get_reg(cursor, UNW_REG_IP, &ip);
    ipp = (void*) ip;
    TMSG(LIBUNW_TRACE, "Init Cursor correction step %d: cursor ip = %p", i+1, ipp);
  }
  h_cursor->sp = h_cursor->bp = NULL;

  hpcrun_unw_get_ip_reg(h_cursor, &ipp);
  
  h_cursor->pc_unnorm = ipp;
  h_cursor->intvl = &(h_cursor->real_intvl);
  h_cursor->intvl->lm = NULL;
  h_cursor->pc_norm = hpcrun_normalize_ip(ipp, NULL);
}

//
// fence checking function
//

static fence_enum_t
hpcrun_check_fence(void* ip)
{
  fence_enum_t rv = FENCE_NONE;
  if (monitor_unwind_process_bottom_frame(ip))
    rv = FENCE_MAIN;
  else if (monitor_unwind_thread_bottom_frame(ip))
    rv = FENCE_THREAD;

   if (ENABLED(FENCE_UNW) && rv != FENCE_NONE)
     TMSG(FENCE_UNW, "%s", fence_enum_name(rv));
   return rv;
}

static bool
fence_stop(fence_enum_t fence)
{
  return (fence == FENCE_MAIN) || (fence == FENCE_THREAD);
}

// ----------------------------------------------------------
// hpcrun_unw_step: 
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame.  Conforms to the semantics of libunwind's
//   hpcrun_unw_step.  In particular, returns:
//     > 0 : successfully advanced cursor to next frame
//       0 : previous frame was the end of the unwind
//     < 0 : error condition
// ---------------------------------------------------------

step_state
hpcrun_unw_step(hpcrun_unw_cursor_t* c)
{
  void* ip;
  int ret;

  hpcrun_unw_get_ip_reg(c, &ip);
  
  
  c->fence = hpcrun_check_fence(c->pc_unnorm);
  if (fence_stop(c->fence)) {
    TMSG(UNW,"unw_step: STEP_STOP, current pc in monitor fence pc=%p\n", c->pc_unnorm);
    return STEP_STOP;
  }
  else {
    ret = unw_step(&(c->uc));
    unw_word_t tmp;
    unw_get_reg(&(c->uc), UNW_REG_IP, &tmp);
    c->pc_unnorm = (void*) tmp;
    c->pc_norm = hpcrun_normalize_ip(c->pc_unnorm, NULL);
    // hackish error check
    if ( ! c->pc_norm.lm_id ) ret = -1;
    c->sp = c->bp = NULL;
    c->intvl = &(c->real_intvl);
    c->intvl->lm = NULL;
    TMSG(UNW, "(libunw) step gives pc=%p", c->pc_unnorm);
    ret = (ret < 0) ? STEP_ERROR : ret;
    TMSG(UNW, "(libunw) step ret=%d", ret);
  }
  return ret;
}

//************************************************
//  additional "support" functions
//************************************************

void
hpcrun_release_splay_lock(void) {
  ;
}

void
hpcrun_delete_ui_range(void* start, void* end)
{
  ;
}

