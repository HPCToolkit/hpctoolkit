#include <excpt.h>
#include <dlfcn.h>

#include "libc.h"
#include "general.h"
#include "util.h"
#include "interface.h"

/* libexc functions--mostly hooks into the exception processing system */
#ifdef CSPROF_FIXED_LIBCALLS
/* the prototype for this function is technically

     exc_continue(CONTEXT *)

   but in staring at code in the debugger, it seems that the CONTEXT
   pointer is really passed in the second argument register (not
   entirely sure what the first one is for...), so we fake things
   to reflect this fact */
static void (*csprof_exc_continue)(CONTEXT *, CONTEXT *)
    = 0x3ff807e21a0;
static unsigned long (*csprof_exc_dispatch_exception)(system_exrec_type *, CONTEXT *)
    = 0x3ff807e2cd0;
/* need to intercept this so that we can remove the trampoline if the exception
   dispatcher happens to wander over a swizzled frame. */
unsigned long (*csprof_exc_virtual_unwind)(PRUNTIME_FUNCTION, CONTEXT *)
    = 0x3ff807e3f90;
#else
static void (*csprof_exc_continue)(CONTEXT *, CONTEXT *);
static unsigned long (*csprof_exc_dispatch_exception)(system_exrec_type *, CONTEXT *);
unsigned long (*csprof_exc_virtual_unwind)(PRUNTIME_FUNCTION, CONTEXT *);
#endif

void
arch_libc_init()
{
#ifndef CSPROF_FIXED_LIBCALLS
    /* specious generality which might be needed later */
#define FROB(our_name, platform_name) \
do { \
    csprof_ ## our_name = dlsym(RTLD_NEXT, #platform_name); \
    if(csprof_ ## our_name == NULL) { \
        printf("Error in locating " #platform_name "\n"); \
        exit(0); \
    } \
} while(0)

    /* exception handling in C and C++ needs this */
    FROB(exc_continue, __exc_continue);
    FROB(exc_dispatch_exception, exc_dispatch_exception);
    FROB(exc_virtual_unwind, exc_virtual_unwind);
#endif
}

/* `nlx' = `non-local-exit'; this function handles the guts of when we
   do longjmps or similar operations (exception handling comes to mind).
   signals should be appropriately masked before entering this function */
void static
do_nlx_twiddling(csprof_state_t *state, CONTEXT *ctx)
{
    void *pc;
    void *sp;
    void *return_address;

    /* figure out where we're heading back to */
    pc = (void *)ctx->sc_pc;
    sp = (void *)ctx->sc_regs[30];

    /* fixup madness.  this may not be necessary anymore, since I suspect
       this occurred if we swizzled the trampoline inside setjmp--an
       interesting thing, to be sure, but something we now block via a
       little conditional logic in the sample-taking process. */
    if(pc == CSPROF_TRAMPOLINE_ADDRESS) {
        /* don't ask */
        DBGMSG_PUB(1, "Changing pc of longjmp env");
        pc = ctx->sc_pc = csprof_bt_top_ip(state);
        DBGMSG_PUB(1, "Data at $sp %p = %p",
                   state->swizzle_patch, *(state->swizzle_patch));
        *(state->swizzle_patch) = state->swizzle_return;
    }

    DBGMSG_PUB(1, "longjmp'ing to %#lx", pc);

    /* what we want to do is move the trampoline into the function to
       which the longjmp is, well, jumping. */

    /* if we don't have a cached call stack of any sort, then we've
       never inserted a trampoline.  don't bother */
    if(!csprof_state_has_empty_backtrace(state)) {
        pdsc_rpd *rpd;
        pdsc_crd *crd;

        csprof_get_pc_rpd_and_crd(pc, &rpd, &crd);

        /* remember that we're storing *canonicalized* stack pointers
           in the backtrace.  canonicalize this one, too */
        sp = VPTR_ADD(sp, PDSC_RPD_SIZE(rpd));
        DBGMSG_PUB(1, "canon $sp: %p", sp);

        if(sp < csprof_bt_top_sp(state)) {
            /* we've made a bunch of calls since the last sample.
               update the backtrace to be consistent with our
               current call chain.  but don't record a sample
               here, since a signal didn't go off (we could probably
               get away with it, but we'll let it slide for now). */
            /* don't bother moving the trampoline for the time being */
            DBGMSG_PUB(1, "Returning to $sp: %p", sp);
#if 0
            csprof_sample_callstack(state, 0, ctx);
#endif
        }
        else if(sp == csprof_bt_top_sp(state)) {
            /* do nothing */
            DBGMSG_PUB(1, "No unwind or re-swizzling required");
            DBGMSG_PUB(1, "swizzled:  %p @ %p = %p",
                       *state->swizzle_patch, state->swizzle_patch, state->swizzle_return);
        }
        else {
            /* undo the current swizzled state */
            if(csprof_swizzle_patch_is_address(state)) {
                DBGMSG_PUB(1, "swizzled state: %p @ %p",
                           state->swizzle_return, state->swizzle_patch);
                *(state->swizzle_patch) = state->swizzle_return;

                state->swizzle_patch = 0;
                state->swizzle_return = 0;
            }
            else {
                ERRMSG("Unable to unswizzle: %p!", __FILE__, __LINE__, state->swizzle_patch);
            }

            DBGMSG_PUB(1, "Unwinding to $sp: %p", sp);
            /* unwind 'til stack pointers match */
            while((csprof_bt_top_sp(state) != sp)
                  && !csprof_state_has_empty_backtrace(state)) {
                csprof_bt_pop(state);
            }

            DBGMSG_PUB(1, "cached stack after unwind: %p/%p",
                       csprof_bt_top_ip(state), csprof_bt_top_sp(state));
            DBGMSG_PUB(1, "cached stack after unwind: %p/%p",
                       csprof_bt_ntop_ip(state), csprof_bt_ntop_sp(state));

            /* now, we might have register frame procedures lurking in
               the midst of our call chain.  this is where longjmp gets
               expensive, but longjmps should be relatively infrequent,
               so we're willing to take a hit here */
            /* FIXME: implement; assume for the time being this
               is impossible (semi-justified assumption, actually) */

            /* swizzle the context to which we're returning */
            /* this is slow, but it's definitely correct */
            csprof_swizzle_with_context(state, ctx);

            DBGMSG_PUB(1, "longjmp swizzled %p @ %p\n",
                       state->swizzle_return, state->swizzle_patch);
        }
    }
    else {
        DBGMSG_PUB(1, "No cached call stack or trampoline inserted");
    }
}

void csprof_nlx_to_jmp_buf(csprof_state_t *state, jmp_buf env)
{
    do_nlx_twiddling(state, (CONTEXT *) &env[0]);
}

void csprof_nlx_to_exception_context(csprof_state_t *state, void *context)
{
    do_nlx_twiddling(state, (CONTEXT *)context);
}

/* see the declaration for `csprof_exc_continue' as to why our prototype
   doesn't match the one you find in the manpage */
void
__exc_continue(CONTEXT *fake, CONTEXT *real)
{
    csprof_state_t *state;
    sigset_t oldset;

    csprof_sigmask(SIG_BLOCK, &prof_sigset, &oldset);

    state = csprof_get_state();

    csprof_state_flag_clear(state, CSPROF_EXC_HANDLING);

    do_nlx_twiddling(state, real);

    DBGMSG_PUB(1, "Preparing to exc_continue...");

    csprof_sigmask(SIG_SETMASK, &oldset, NULL);
    libcall2(csprof_exc_continue, fake, real);
}

/* `exc_dispatch_exception' unwinds the stack to search for an exception
   handler for the current exception.  unfortunately, if the trampoline's
   in the way, then the unwind will hork.  we handle this by temporarily
   removing the trampoline and setting a flag to let the signal handler
   know that taking samples and moving the trampoline is a Bad Idea. */
unsigned long
exc_dispatch_exception(system_exrec_type *excrec, CONTEXT *ctx)
{
    csprof_state_t *state;
    sigset_t oldset;

    csprof_sigmask(SIG_BLOCK, &prof_sigset, &oldset);

#ifndef CSPROF_FIXED_LIBCALLS
    if(csprof_exc_dispatch_exception == NULL) {
        FROB(exc_dispatch_exception, exc_dispatch_exception);
    }
#endif

    state = csprof_get_state();

    /* note our current state */
    csprof_state_flag_set(state, CSPROF_EXC_HANDLING);

    csprof_sigmask(SIG_SETMASK, &oldset, NULL);
    /* do the normal thing */
    return libcall2(csprof_exc_dispatch_exception, excrec, ctx);
}

unsigned long
exc_virtual_unwind(pdsc_crd *prf, CONTEXT *ctx)
{
    csprof_state_t *state = csprof_get_state();
    unsigned long ret = libcall2(csprof_exc_virtual_unwind, prf, ctx);

    if(ctx->sc_pc == CSPROF_TRAMPOLINE_ADDRESS) {
        ctx->sc_pc = state->swizzle_return;
    }

    return ret;
}

