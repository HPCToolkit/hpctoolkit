/* functions operating on thread-local state */
#ifndef CSPROF_STATE_H
#define CSPROF_STATE_H

#include "structs.h"

/* getting and setting states independent of threading support */
csprof_state_t *csprof_get_state();
void csprof_set_state(csprof_state_t *);

/* initialize various parts of a state */
int csprof_state_init(csprof_state_t *);
/* initialize dynamically allocated portions of a state */
int csprof_state_alloc(csprof_state_t *);

extern csprof_state_t *csprof_check_for_new_epoch(csprof_state_t *);

/* expand the internal backtrace buffer */
csprof_frame_t *csprof_state_expand_buffer(csprof_state_t *, csprof_frame_t *);
int csprof_state_insert_backtrace(csprof_state_t *, int, csprof_frame_t *,
                                  csprof_frame_t *, size_t);
#if defined(CSPROF_PERF)
#define csprof_state_verify_backtrace_invariants(state)
#else
#define csprof_state_verify_backtrace_invariants(state) \
do { \
    int condition = (state->btbuf < state->bufend) /* obvious */\
        && (state->btbuf <= state->bufstk) /* stk between beg and end */\
        && (state->bufstk <= state->bufend);\
\
    if(!condition) {\
        DIE("Backtrace invariant violated", __FILE__, __LINE__);\
    }\
} while(0);
#endif

/* finalize various parts of a state */
int csprof_state_fini(csprof_state_t *);
/* destroy dynamically allocated portions of a state */
int csprof_state_free(csprof_state_t *);

#define csprof_state_has_empty_backtrace(state) ((state)->bufend - (state)->bufstk == 0)
#define csprof_bt_pop(state) do { state->bufstk++; } while(0)
#define csprof_bt_top_ip(state) (state->bufstk->ip)
#define csprof_bt_top_sp(state) (state->bufstk->sp)
/* `ntop' == `next top'; any better ideas? */
#define csprof_bt_ntop_ip(state) ((state->bufstk + 1)->ip)
#define csprof_bt_ntop_sp(state) ((state->bufstk + 1)->sp)


/* various flag values for csprof_state_t; pre-shifted for efficiency and
   to enable set/test/clear multiple flags in a single call */
#define CSPROF_EXC_HANDLING (1 << 0)   /* true while exception processing */
#define CSPROF_TAIL_CALL (1 << 1)      /* true if we're unable to swap tramp;
                                          usually this only happens when tail
                                          calls occur */
#define CSPROF_THRU_TRAMP (1 << 2)     /* true if we've been through a trampoline
                                          since the last signal we hit */
#define CSPROF_BOGUS_CRD (1 << 3)      /* true if we discovered from
                                          the unwind process that the
                                          current context's CRD is
                                          totally bogus
                                          (e.g. NON_CONTEXT for a
                                          perfectly ordinary stack
                                          frame procedure...) */
/* true if we discovered during the unwinding that the return address has
   already been reloaded from the stack */
#define CSPROF_EPILOGUE_RA_RELOADED (1 << 4)
/* true if we discovered during the unwinding that the stack/frame pointer
   has been resest */
#define CSPROF_EPILOGUE_SP_RESET (1 << 5)
/* true if we received a signal whilst executing the trampoline */
#define CSPROF_SIGNALED_DURING_TRAMPOLINE (1 << 6)
/* true if this malloc has realloc in its call chain */
#define CSPROF_MALLOCING_DURING_REALLOC (1 << 7)
static inline int
csprof_state_flag_isset(csprof_state_t *state, unsigned int flag)
{
    unsigned int state_flags = state->flags;

    return state_flags & flag;
}

static inline void
csprof_state_flag_set(csprof_state_t *state, unsigned int flag)
{
    state->flags = state->flags | flag;
}

static inline void
csprof_state_flag_clear(csprof_state_t *state, unsigned int flag)
{
    state->flags = state->flags & (~flag);
}

/* various macros to determine the state of the trampoline */
/* we have several different conventions for what state->swizzle_patch does.
   it would probably be more helpful to separate them into distinct
   variables inside the csprof_state_t structure, but right now they're
   here to stay.  and anyway, if we did that, we'd have the same number
   of functions, because we'd constantly have to be disambiguating which
   member we actually want to access.

   here are the relevant values:

   + state->swizzle_patch == 0

     we haven't swizzled anything yet; swizzle_patch should have this
     value when we start up the library and at no other time.  except
     perhaps when we return from a longjmp and get confused, or when
     we return to main() through a trampoline and don't install a trampoline
     to catch the return to __start.

   + 0 < state->swizzle_patch < CSPROF_SWIZZLE_REGISTER

     the upper bound is chosen more-or-less randomly; we just need it to
     be an invalid memory address for the program to access.

     swizzle_patch holds the number of a machine register which held
     the return address when we swizzled.  we assume that nobody is
     ever going to place their return address in $v0.  that would be evil.

   + state->swizzle_patch >= CSPROF_SWIZZLE_REGISTER

     state->swizzle_patch is an actual dereferenceble address that held the
     address to which we should be returning. */

#ifdef CSPROF_TRAMPOLINE_BACKEND

#define CSPROF_SWIZZLE_REGISTER ((void **)41)

static inline int
csprof_swizzle_patch_is_valid(csprof_state_t *state)
{
    return state->swizzle_patch != 0;
}

static inline int
csprof_swizzle_patch_is_register(csprof_state_t *state)
{
    return (((void **)0) < state->swizzle_patch)
        && (state->swizzle_patch < CSPROF_SWIZZLE_REGISTER);
}

static inline int
csprof_swizzle_patch_is_address(csprof_state_t *state)
{
    return CSPROF_SWIZZLE_REGISTER <= state->swizzle_patch;
}
#endif

#endif
