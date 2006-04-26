/* exc_backtrace.c -- unwinding the stack with HP's libexc */

#include <stddef.h>
#include <excpt.h>

#include "backtrace.h"
#include "general.h"
#include "list.h"
#include "libc.h"
#include "util.h"
#include "csprof_csdata.h"
#include "alpha_inst.h"

extern void csprof_get_primary_rpd(void *, pdsc_rpd **);
extern void csprof_get_pc_rpd_and_crd(void *, pdsc_rpd **, pdsc_crd **);

/* FIXME, this is getting ugly */
/* FIXME: duplication */
extern void *csprof_trampoline;

void
csprof_record_metric_with_unwind(int metric_id, size_t value, int unwinds)
{
  int ret;
  ucontext_t context;

  ret = getcontext(&context);

  if(ret != 0) {
    int i;

    for(i = 0; i < unwinds; ++i) {
      libcall2(csprof_exc_virtual_unwind, 0, &(context.uc_mcontext));
    }

    /* amazingly, we still have to check for this... */
    if(!csprof_context_is_unsafe(&context)) {
      csprof_sample_callstack(state, metric_id, value,
			      &(context.uc_mcontext));
    }
  }

#define CSPROF_TRAMPOLINE_LOC ((void *)&csprof_trampoline)

/* also from Paul Drongowski */
static int
csprof_backtrace_done(CONTEXT *ctx, unsigned long *pcp, unsigned long *spp)
{
    unsigned long pc = *pcp;
    unsigned long sp = *spp;

    /* sc_pc is supposed to be zero when we reached the top of the
       unwind stack.  seems like a logical choice */
    if(ctx->sc_pc == 0
#ifdef CSPROF_THREADS
       /* threads in tru64 appear to trampoline out of __thdBase
          in libpthread.so.  Unwinds from this procedure produce
          stack pointers of zero.  We're pretty sure this wouldn't
          happen anyplace else, but this piece of code is #ifdef'd
          just to make sure. */
       || ctx->sc_regs[30] == 0
#endif
       ) {
        return 1;
    }
    else if((ctx->sc_pc == pc) && (ctx->sc_regs[30] == sp)) {
        DBGMSG_PUB(1, "stop due to ==");
        return 1;
    }
    else {
        *spp = ctx->sc_regs[30];
        *pcp = ctx->sc_pc;

        return 0;
    }
}


#if 0
/* maintaining a cache of "known good" addresses, where we don't have to
   perform any unwind fixup checking */
#define CACHE_SIZE 16

static int pos = 0;
static int wrapped = 0;
static void *known_good_unwind_addresses[CACHE_SIZE];

static int
is_known_good(void *addr)
{
    int i;

    for(i=0; i<(wrapped ? CACHE_SIZE : pos); ++i) {
        void *x = known_good_unwind_addresses[i];
        if(x == addr) {
            return 1;
        }
    }

    return 0;
}

static void
push_known_good(void *addr)
{
    int next_pos = (pos + 1) % CACHE_SIZE;
    if(next_pos < pos) {
        wrapped = 1;
    }
    pos = next_pos;
    known_good_unwind_addresses[next_pos] = addr;
}
#endif


/* clever return values for csprof_backtrace */
#define CSPROF_TARGET1 3
#define CSPROF_TARGET2 4

/* Note: based on backtrace() from libunwind. */
static int
csprof_backtrace(csprof_state_t *state, csprof_frame_t **unwind_ptr, void *context)
{
    CONTEXT ctx;
    /* done because C is stupid */
    CONTEXT *contextptr = (CONTEXT *)context;
    /* figuring out if we're done unwinding */
    unsigned long program_counter = 0, stack_pointer = 0;

    /* true if we had to modify the context because of poor compilers */
    int in_epilogue;
    /* we should only have to muck with the context the first time through */
    int first_unwind = 1;
    /* true if we did muck with the context the first time through */
    int did_muck = 0;

    int status;
    csprof_frame_t *unwind = *unwind_ptr;
    void *pc1 = state->bufstk->ip;
    void *sp1 = state->bufstk->sp;

    void *ip;
    void **canon_sp;
    unsigned int ra_register, i;
    pdsc_rpd *rpd = NULL, *t1rpd, *t2rpd;
    pdsc_crd *crd = NULL, *t1crd;

#define ADD_FRAME(iptr, sptr) { \
unwind->ip = iptr; \
unwind->sp = sptr; \
++unwind; \
}

    memcpy(&ctx, context, sizeof(CONTEXT));

    /* we aren't "done"; this step is merely to initialize the pc
       and the sp correctly.  if we actually are done, this is bogus */
    if(csprof_backtrace_done(&ctx, &program_counter, &stack_pointer)) {
        /* weird */
        ERRMSG("Backtrace complete without unwinding",
               __FILE__, __LINE__);
        ERRMSG("ctx pc: %#lx, ctx sp: %#lx\n"
               "\tpc: %#lx, sp: %#lx",
               __FILE__, __LINE__,
               ctx.sc_pc, ctx.sc_regs[30], program_counter, stack_pointer);

        status = CSPROF_ERR;
        goto BACKTRACE_EXIT;
    }

    /* obtain the rpd for the procedures we want to look at */
    if(state->bufstk == state->bufend) {
        t1crd = t1rpd = NULL;
    }
    else {
        csprof_get_pc_rpd_and_crd(pc1, &t1rpd, &t1crd);
    }

    DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Targets: %#lx %#lx %#lx %#lx",
               pc1, sp1, t1rpd, t1crd);

    while(1) {
        int frame_size, prologue_len, sp_set_offset, frame_reg;
        void *func_begin;
        void *func_body_begin;
        void *sp_set_pc;

        ip = (void *)ctx.sc_pc;
        in_epilogue = 0;

        if(unwind == state->bufstk) {
            /* bumping up against the already-recorded stack */
            unwind = csprof_state_expand_buffer(state, unwind);
        }

        if(ip == CSPROF_TRAMPOLINE_LOC) {
	    /* uh oh, tail call.  must do a few things: */
	    /* 1. fix the context */
            ip = state->swizzle_return;
	    ctx.sc_pc = state->swizzle_return;

	    /* 2. pop cached call stack */
	    state->bufstk++;
	    {
		csprof_cct_node_t *tn = state->treenode;
                assert(tn != NULL);
                tn->calls++;
                state->treenode = tn->parent;
	    }

	    /* 3. move targets around to reflect reality */
            pc1 = csprof_bt_top_ip(state);
            sp1 = csprof_bt_top_sp(state);
            csprof_get_pc_rpd_and_crd(pc1, &t1rpd, &t1crd);

            /* 4. remove the trampoline from its previous location

               rpd is the procedure descriptor for the *previous* iteration */
            if(rpd != NULL && !PDSC_RPD_REGISTER(rpd)) {
                /* neat feature of this point: sp is located where it
                   would be when the previous iteration's function was
                   called, so we can index off of it safely without
                   having to worry about weird things like whether the
                   previous procedure used $fp or $sp and whether or not
                   those values had been restored and so forth... */
                void **sp = ctx.sc_regs[30];
                unsigned int rsa_offset = PDSC_RPD_RSA_OFFSET(rpd);
                int frame_size = PDSC_RPD_SIZE(rpd);
                sp = VPTR_ADD(sp, -frame_size);
                void **ra_loc = (void **)VPTR_ADD(sp, rsa_offset);

                DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL, "Computed raloc %#lx", ra_loc);

                if(*ra_loc = CSPROF_TRAMPOLINE_LOC) {
                    DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL, "Replaced trampoline at $sp %#lx = %#lx", sp, ra_loc);
                    *ra_loc = state->swizzle_return;
                }
                else {
                    DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL, "Unable to locate replaceable trampoline");
                }
            }
            else {
                int ra_register = PDSC_RPD_SAVE_RA(rpd);

                if(contextptr->sc_regs[ra_register] == CSPROF_TRAMPOLINE_LOC) {
                    DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL,
                               "Unswizzling register %d", ra_register);
                    contextptr->sc_regs[ra_register] = state->swizzle_return;
                }

                ra_register = PDSC_RPD_ENTRY_RA(rpd);

                if(contextptr->sc_regs[ra_register] == CSPROF_TRAMPOLINE_LOC) {
                    DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL,
                               "Unswizzling register %d", ra_register);
                    contextptr->sc_regs[ra_register] = state->swizzle_return;
                }
            }

	    /* 5. make some noise */
            DBGMSG_PUB(CSPROF_DBG_UNW_TAILCALL, "pc changed: %#lx!", state->swizzle_return);
        }

        /* obtain information about the function */
        csprof_get_pc_rpd_and_crd(ip, &rpd, &crd);

#if 0
        DBGMSG_PUB(CSPROF_DBG_UNWINDING, "ip before fiddling: %#lx", ip);
#endif

        frame_size = PDSC_RPD_SIZE(rpd);
        func_begin = (void *)PDSC_CRD_BEGIN_ADDRESS(exc_lookup_function_table((exc_address)ip), crd);
        sp_set_offset = PDSC_RPD_SP_SET(rpd);
        sp_set_pc = VPTR_ADD(func_begin, sp_set_offset);
        prologue_len = PDSC_RPD_ENTRY_LENGTH(rpd);
        func_body_begin = VPTR_ADD(func_begin, prologue_len);

        /* PDSC_RPD_SAVE_RA has a sensible default if rpd == NULL */
        ra_register = PDSC_RPD_SAVE_RA(rpd);

        if(first_unwind && PDSC_RPD_SIZE(rpd)) {
            /* check for violations of the calling conventions in stack frame
               procedures: the register holding the return address should
               not be modified during the prologue.  if the application
               does modify it and we attempt to unwind with the modified
               register, the unwinder will take the current contents as
               gospel, producing very screwy backtraces. */
            if((sp_set_pc < ip) && (ip < func_body_begin)
               /* if the code range doesn't contain the prologue,
                  don't bother checking what's going on, because there
                  cannot be a violation */
               && PDSC_CRD_TYPE_STANDARD(crd)
               /* particular stupidity only occurs with stack frame procedures */
               && (rpd != NULL) && !PDSC_RPD_REGISTER(rpd)) {
                /* we're in the prologue, check for stupidity.

                   $sp is pointing at the bottom of the stack for sure.
                   find the return address slot */
                void **sp = (void **)ctx.sc_regs[30];

                /* sometimes (256.bzip2), the compiler lies to us about
                   the stack usage of a procedure.  sanity check things
                   to ensure we're not totally deceived */
                unsigned int spset_inst = *(unsigned int *)sp_set_pc;

                /* csprof_inst_is_lda was really designed for checking
                   epilogues, not prologues--and the offset in epilogues
                   is positive, whereas in prologues it's negative.  so
                   compensate by creating a "negative" frame size in
                   two's complement notation */
                if((csprof_inst_is_lda(spset_inst, (65536 - frame_size))
                    || csprof_inst_is_addq(spset_inst))) {
                    unsigned int *stqra_inst_loc =
                        csprof_find_stqra_inst(ip, func_body_begin);

                    if(stqra_inst_loc == func_body_begin) {
                        /* we've stored the return address into the stack frame.
                           compute its location, retrieve it, and compare it to
                           the current contents of the entry register */
                        void **raloc = VPTR_ADD(sp, PDSC_RPD_RSA_OFFSET(rpd));
                        void *ra = *raloc;

                        if(ra == ((void *)ctx.sc_regs[26])) {
                            /* good, nothing to do here */
                        }
                        else {
                            /* danger, Will Robinson!  shove the real return
                               address back into the context */
                            DBGMSG_PUB(CSPROF_DBG_UNWINDING,
                                       "func_body_begin=%#lx, sp_set_pc=%#lx", func_body_begin, sp_set_pc);
                            DBGMSG_PUB(CSPROF_DBG_UNWINDING,
                                       "Modifying $ra in context from %#lx", ctx.sc_regs[26]);
                            ctx.sc_regs[26] = ra;
                            did_muck = 1;
                        }
                    }
                    else {
                        /* nothing to do; the return address register must be good */
                        DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Found stqra @ %#lx", stqra_inst_loc);
                    }
                }
                else {
                    DBGMSG_PUB(CSPROF_DBG_UNWINDING, "spset doesn't move stack: %x", spset_inst);
                }
            }
            else {
                /* might have to fixup inattention to the calling
                   conventions where the lda sp, <framesize>(sp) instruction
                   does not occur immediately before the ret instruction.
               
                   we used to do this farther down in the function, but we
                   take care of it here so that we can get accurate stack
                   pointer information for figuring out where we are in the
                   backtrace; relying on the stack pointer to disambiguate
                   things does *not* help if your stack pointer is bogus. */

                /* what a pain */
                unsigned int *instptr = (unsigned int *)ip;
                unsigned int *p;
                unsigned int flag = 0;  /* have we been through the loop? */

                /* actually, this should be done by the unwinder for us
                   in super-recent versions of Tru64, since the unwinding
                   info records situations like these.  but we have this
                   support here now in order to work on not-so-super-recent
                   systems */
                /* should this be the same as EPILOGUE_INSTRUCTIONS_GUESS? */
                /* don't even ask why this number is so freakin' huge */
#define BACKTRACE_EPILOGUE_GUESS 80
                /* scan forward through the following instructions.  if
                   we encounter a lda sp instruction, then we know nothing
                   needs to be done.  if we encounter a ret instruction,
                   we need to figure out the frame size, *subtract* it from
                   the appropriate register, and break.  should this be
                   its own function? */
                for(p = instptr; p<(instptr+BACKTRACE_EPILOGUE_GUESS); ++p) {
                    unsigned int inst = *p;
                    pdsc_rpd *xrpd;
                    pdsc_crd *xcrd;

                    if((inst >> 16 == 0xa75e) /* ldq ra, ...(sp) */
                       || (inst >> 16 == 0xa74f)) { /* ldq ra, ...(fp) */
                        /* check to see whether this instruction lies in
                           a code range which is not supposed to have
                           stack allocation performed.  the mere presence
                           of this instruction in such a range indicates
                           that something is *very* wrong.  nevertheless,
                           we need to fix it up here, since the unwinder
                           needs such help */
                        DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Breaking out of loop due to reload");

                        csprof_get_pc_rpd_and_crd(p, &xrpd, &xcrd);
                        if(PDSC_CRD_TYPE_NON_CONTEXT(crd) && crd == xcrd) {
                            DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Fixing up non-context idiocy");
                            int load_reg = inst >> 16 & 0x1f;
                            /* FIXME: figure out how to properly do
                               negative offsets */
                            void **ptr = (void **)(ctx.sc_regs[load_reg] + (inst & 0xffff));

                            ctx.sc_regs[26] = (long)*ptr;
                            /* nonobvious...dangerous for $fp procedures */
                            ctx.sc_regs[30] += PDSC_RPD_SIZE(rpd);
                            did_muck = 1;
                        }

                        break;
                    }
                    else if(csprof_inst_is_lda(inst, frame_size) || csprof_inst_is_addq(inst)) {
                        /* check to see whether this instruction lies in
                           a code range which is not supposed to have
                           stack allocation performed.  if so, and if the
                           code range is identical to the code range of the
                           context, modify the context to deallocate the
                           stack automatically. */
                        csprof_get_pc_rpd_and_crd(p, &xrpd, &xcrd);

                        if(PDSC_CRD_TYPE_NON_CONTEXT(crd) && crd == xcrd) {
                            /* deallocate stack */
                            DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Deallocating stack for bogus code range");
                            ctx.sc_regs[30] += PDSC_RPD_SIZE(rpd);
                        }
                        csprof_state_flag_set(state, CSPROF_EPILOGUE_RA_RELOADED);
                        DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Breaking out of the loop at %#lx", p);
                        did_muck = 1;
                        break;
                    }
                    /* don't run forward over backwards branches */
                    else if(csprof_inst_is_backwards_branch(inst)) {
                        DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Branching out of the loop at %#lx", p);
                        break;
                    }
                    else if(csprof_inst_is_return(inst)) {
                        if(flag == 0) {
                            /* we started at a ret, the unwinder
                               knows what to do in this case */
                            goto RA_HANDLE;
                        }
                        DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Landed in the return case %#lx", p);
                        /* whoops!  somebody forgot something */
                        if(PDSC_RPD_BASE_REG_IS_FP(rpd)) {
                            /* don't think we can handle this...but maybe
                               I haven't thought about it hard enough */
#if 1
                            ERRMSG("Bug the developer to fix things",
                                   __FILE__, __LINE__);
#endif
                        }
                        else {
                            /* we move the instruction pointer to point at
                               the return instruction rather than fiddling
                               with the stack pointer.  one reason to do
                               things this way is we have guarantees about
                               the state of the machine when the pc is
                               pointing at a return instruction: everything
                               is as it was before the current function
                               was called.

                               the second reason is that the unwinder is
                               smart enough to figure out what's happening
                               in this case; it is not always smart enough
                               to discern what happens if we adjust the
                               stack pointer (because we may be severly
                               messing with things if we do so) */
                            DBGMSG_PUB(CSPROF_DBG_UNW_SPRET, "Adjusting the instruction pointer");
                            ctx.sc_pc = (long)p;
                            goto RA_HANDLE;
                        }

                    RA_HANDLE:
                        csprof_state_flag_set(state, CSPROF_EPILOGUE_SP_RESET);
                        in_epilogue = 1;
                        did_muck = 1;
                        break;
                    }

                    flag = 1;
                }
            }
        }
#if 0
        /* check to see if the context was flagged down by our sanity
           checking.  if it wasn't, mark it as good */
        if(first_unwind && !did_muck) {
            push_known_good(ip);
        }
#endif
        /* attempt to canonicalize the stack pointer so that e.g. we
           don't get proc1->proc2 twice in the same graph because
           one of them happened to occur when the stack pointer *hadn't*
           been moved and one of them occurred when the stack pointer
           had been moved.  we choose, perhaps arbitrarily, the stack
           pointer upon entrance to the procedure to be "canonical".
           this choice makes it easier to determine when and by how much
           the stack pointer needs to be adjusted. */
        if(!first_unwind) {
            /* we're guaranteed to be at a "known good" spot; just
               increase the stack pointer by the frame size and go */
            frame_reg = ((rpd == NULL) ? 30 : (PDSC_RPD_BASE_REG_IS_FP(rpd) ? 15 : 30));

            canon_sp = (void **)(ctx.sc_regs[frame_reg] + PDSC_RPD_SIZE(rpd));
        }
        else if(in_epilogue) {
            /* no adjustment required */
            canon_sp = (void **)ctx.sc_regs[30];
        }
        else {
            if(rpd == NULL) {
                /* register procedure: no adjustment required */
                canon_sp = (void **)ctx.sc_regs[30];
            }
            else {
                /* the fun case; this is expensive, but we should
                   only be doing it at most once per unwind */
                /* must check for STANDARD CRDs to avoid spurious spset
                   instructions showing up */
                if(ip <= sp_set_pc && PDSC_CRD_TYPE_STANDARD(crd)) {
                    canon_sp = (void **)ctx.sc_regs[30];
                }
                else if(csprof_inst_is_return(*((unsigned int *) ip))) {
                    /* we're at a return instruction *or* we haven't
                       moved the stack pointer (so we're still somewhere
                       in the prologue.  the stack pointer is still good. */
                    canon_sp = (void **)ctx.sc_regs[30];
                }
                else if(ip < func_body_begin && PDSC_CRD_TYPE_STANDARD(crd)) {
                    canon_sp = (void **)(ctx.sc_regs[30] + PDSC_RPD_SIZE(rpd));
                }
                else {
                    /* we're past the $sp set instruction.  add back
                       the frame size */
                    int frame_reg = PDSC_RPD_BASE_REG_IS_FP(rpd) ? 15 : 30;

                    canon_sp = (void **)(ctx.sc_regs[frame_reg] + PDSC_RPD_SIZE(rpd));
                }
            }
        }

        DBGMSG_PUB(CSPROF_DBG_UNWINDING,
                   "unwind pc %#lx | sp %#lx | canon %#lx",
                   ip, ctx.sc_regs[30], canon_sp);

#if 0
        if(crd == t1crd) {
            if(canon_sp == sp1) {
                if(ip == pc1) {
                    status = CSPROF_TARGET1;
                    goto BACKTRACE_EXIT;
                }
                else {
                    ADD_FRAME(ip, canon_sp);
                    status = CSPROF_TARGET2;
                    goto BACKTRACE_EXIT;
                }
            }
            else {
                goto record_sample;
            }
        }
        else {
            goto record_sample;
        }
#else
        /* if target 1 is a null frame procedure, then we shouldn't
           have to check anything about it... right? */
        if(t1rpd != NULL) {
            /* t1rpd is not a null frame procedure */
            if(rpd == t1rpd) {
                /* so we're in the same procedure...are we at the
                   same pc?  and even if we are at the same pc,
                   make sure that the stack pointer varies between
                   the two to properly catch recursive calls */
                if(sp1 == canon_sp) {
                    if(pc1 == ip) {
                        /* an exact match */
                        status = CSPROF_TARGET1;
                        goto BACKTRACE_EXIT;
                    }
                    else {
                        /* an almost exact match */
                        ADD_FRAME(ip, canon_sp);
                        status = CSPROF_TARGET2;
                        goto BACKTRACE_EXIT;
                    }
                }
                else {
                    /* the recursive procedure case */
                    goto record_sample;
                }
            }
            /* we're not in the same procedure, continue on */
            else {
                /* for whatever reason, this used to be `second_procedure'
                   and got changed to `record_sample'.  this doesn't make
                   much sense if you think about it hard enough, so I have
                   changed it back (01-10-2004)... */
                /*goto second_procedure;*/
                goto record_sample;
            }
        }
        else {
            /* null frame procedures; check the CRDs instead */
            if(t1crd == crd) {
                DBGMSG_PUB(CSPROF_DBG_UNWINDING, "CRD equivalence");
                status = CSPROF_TARGET1;
                goto BACKTRACE_EXIT;
            }
            else {
                goto record_sample;
            }
        }
#endif

    record_sample:
        /* add it to our backtrace */
        ADD_FRAME(ip, canon_sp);

        libcall2(csprof_exc_virtual_unwind, crd, &ctx);
        if(csprof_backtrace_done(&ctx, &program_counter, &stack_pointer)) {
            status = CSPROF_OK;
            goto BACKTRACE_EXIT;
        }

        first_unwind = 0;
    }

 BACKTRACE_EXIT:
    *unwind_ptr = unwind;

    return status;
}

/* sample the callstack and record the data in our callstack tree.
   returns the standard return values to indicate success or failure. */
int
csprof_sample_callstack(csprof_state_t *state, int metric_id,
			size_t sample_count, void *context)
{
    csprof_frame_t *unwind = state->btbuf;
    csprof_frame_t *start;
    csprof_frame_t *end;

    size_t newsz = 0;

    csprof_state_verify_backtrace_invariants(state);

    int status = csprof_backtrace(state, &unwind, context);

    csprof_state_verify_backtrace_invariants(state);

    if(status == CSPROF_TARGET2) {
        csprof_cct_node_t *treenode = state->treenode;
        assert(treenode != NULL);
        state->treenode = treenode->parent;
        /* pop the frame from the backtrace buffer */
        state->bufstk++;
    }

    csprof_state_verify_backtrace_invariants(state);

    newsz = unwind - state->btbuf;

    /* check that we haven't done something terribly funky */
    if(newsz != 0) {
        csprof_frame_t *recorded_top = unwind - 1;
        csprof_frame_t *cached_top = state->bufend - 1;

#if 0
        DBGMSG_PUB(1, "Saw %d frames", newsz);
        DBGMSG_PUB(1, "btdump buf %#lx | unwind %#lx | stk %#lx | end %#lx",
                   state->btbuf, unwind, state->bufstk, state->bufend);

#if 0
        for(start = state->btbuf; start < unwind; start++) {
            DBGMSG_PUB(1, "Recording %#lx | %#lx", start->ip, start->sp);
        }
        for(start = state->bufstk; start < state->bufend; start++) {
            DBGMSG_PUB(1, "Already present: %#lx | %#lx", start->ip, start->sp);
        }
#endif
#endif

#if (!CSPROF_MALLOC_PROFILING)
        if(recorded_top->ip == cached_top->ip
           && recorded_top->sp == cached_top->sp) {
            fflush(stderr);
            DIE("Eliminate all occurrences of smashing!\n"
                "recorded | cached addr: %#lx | %#lx\n"
                "recorded: %#lx | %#lx / cached: %#lx | %#lx", __FILE__, __LINE__,
                recorded_top, cached_top,
                recorded_top->ip, recorded_top->sp, cached_top->ip, cached_top->sp);

            /* whoa, funky error */
            DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Smashing!");
            memmove(state->bufend-newsz, state->btbuf, newsz*sizeof(csprof_frame_t));
            state->bufstk = state->bufend - newsz;
            start = state->bufend-1;
            end = state->bufstk;
        }
        else
#endif
        {
            /* shove the collected frames together */
            DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Attaching!");
            memmove(state->bufstk-newsz, state->btbuf, newsz*sizeof(csprof_frame_t));
            state->bufstk = state->bufstk-newsz;
            start= state->bufstk+newsz-1;
            end = state->bufstk;
        }
    }
    else {
        /* nothing to record */
        return CSPROF_OK;
    }

#if 0
    DBGMSG_PUB(CSPROF_DBG_UNWINDING,
               "btdump buf %#lx | unwind %#lx | stk %#lx | end %#lx",
               state->btbuf, unwind, state->bufstk, state->bufend);
    DBGMSG_PUB(CSPROF_DBG_UNWINDING,
               "start %#lx | end %#lx\n", start, end);

    {
        csprof_frame_t *foo;

        for(foo = start; foo >= end; foo--) {
            DBGMSG_PUB(CSPROF_DBG_UNWINDING, "Really recording %#lx | %#lx", foo->ip, foo->sp);
        }
    }
#endif

    csprof_state_verify_backtrace_invariants(state);

    /* record the backtrace */
    if(csprof_state_insert_backtrace(state, metric_id,
				     start, end, sample_count) != CSPROF_OK) {
        ERRMSG("failure recording callstack sample", __FILE__, __LINE__);
        return CSPROF_ERR;
    }

    csprof_state_verify_backtrace_invariants(state);

    return CSPROF_OK;
}
