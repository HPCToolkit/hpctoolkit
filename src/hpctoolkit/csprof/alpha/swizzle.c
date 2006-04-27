#include <ucontext.h>
#include <excpt.h>

#include "interface.h"
#include "alpha_inst.h"
#include "general.h"
#include "tramp.h"

#define FP_REG 15
#define SP_REG 30

extern void *__start;
extern void *main;


/* forward declarations */
static int calc_offset_for_register(int reg);
static void find_ra_in_leaf_procedure(csprof_state_t *, struct lox *,
                                      CONTEXT *, pdsc_rpd *, pdsc_crd *);
static void find_ra_in_stack_procedure(csprof_state_t *, struct lox *,
                                       CONTEXT *, pdsc_rpd *, pdsc_crd *);
void csprof_get_pc_rpd_and_crd(void *, pdsc_rpd **, pdsc_crd **);


/* implementation of the necessary profiling interface */

void *
csprof_get_pc(void *context)
{
    CONTEXT *ctx = (CONTEXT *) context;

    return ctx->sc_pc;
}

extern void *csprof_trampoline_end;
extern void *csprof_trampoline2;
extern void *csprof_trampoline2_end;

static int
csprof_in_trampoline_code(void *pc)
{
    void *t1_start = (void *)&csprof_trampoline;
    void *t1_end = (void *)&csprof_trampoline_end;
    void *t2_start = (void *)&csprof_trampoline2;
    void *t2_end = (void *)&csprof_trampoline2_end;

    return ((t1_start <= pc) && (pc <= t1_end))
        || ((t2_start <= pc) && (pc <= t2_end));
}

int
csprof_find_return_address_for_context(csprof_state_t *state,
                                       struct lox *l, mcontext_t *context)
{
    CONTEXT *ctx = (CONTEXT *)context;
    void *ip = (void *)ctx->sc_pc;
    void *trampoline = CSPROF_TRAMPOLINE_ADDRESS;

    /* if we got interrupted in the midst of the trampoline, don't
       bother changing anything, since we're about to hop out */
    /* FIXME: this check may be redundant with some of the other guards
       which have been put in place in various parts of the code */
    if(csprof_in_trampoline_code(ip)) {
        return 0;
    }
    else {
        pdsc_rpd *rpd;
        pdsc_crd *crd;

        csprof_get_pc_rpd_and_crd(ip, &rpd, &crd);

        if((rpd == NULL) || PDSC_RPD_REGISTER(rpd)) {
            find_ra_in_leaf_procedure(state, l, ctx, rpd, crd);
        }
        else {
            find_ra_in_stack_procedure(state, l, ctx, rpd, crd);
        }

        return 1;
    }
}

void *
csprof_replace_return_address_onstack(void **stackptr, int reg)
{
    void **return_address_location = stackptr + calc_offset_for_register(reg);
    void *ra;

    if(*return_address_location != CSPROF_TRAMPOLINE_ADDRESS) {
        ra = *return_address_location;
        *return_address_location = CSPROF_TRAMPOLINE_ADDRESS;
    }
    else {
        DIE("Trampoline return not patched out correctly!", __FILE__, __LINE__);
    }

    return ra;
}

int
csprof_find_return_address_for_function(csprof_state_t *state,
                                        struct lox *l, void **stackptr)
{
    static pdsc_crd *main_crd = NULL;
    pdsc_crd *func_crd;
    void *return_ip = state->swizzle_return;

    if(main_crd == NULL) {
        main_crd = exc_lookup_function_entry((unsigned long)&main);
    }

    func_crd = exc_lookup_function_entry((unsigned long)return_ip);

    if(main_crd == func_crd) {
        /* don't insert a trampoline when returning from 'main' */
        return 0;
    }
    else {
        pdsc_rpd *rpd = PDSC_CRD_PRPD(func_crd);

        if(rpd == NULL) {
            /* null frame procedure */
            l->stored.type = REGISTER;
            l->stored.location.reg = 26;
        }
        else if(PDSC_RPD_REGISTER(rpd)) {
            /* register frame procedure */
            l->stored.type = REGISTER;
            l->stored.location.reg = PDSC_RPD_SAVE_RA(rpd);
        }
        else {
            /* stack frame procedure */
            void **return_sp;
            int rsa_offset = PDSC_RPD_RSA_OFFSET(rpd);

            if(PDSC_RPD_BASE_REG_IS_FP(rpd)) {
                return_sp = (void **)(*(stackptr + calc_offset_for_register(15)));
            }
            else {
                /* FIXME: not sure if TRAMP_STACK_SPACE will survive into
                   the new machine-independent csprof */
                return_sp = stackptr + TRAMP_STACK_SPACE/sizeof(void *);
            }

            return_sp = VPTR_ADD(return_sp, rsa_offset);

            l->stored.type = ADDRESS;
            l->stored.location.address = return_sp;
        }

        return 1;
    }
}

void
csprof_remove_trampoline(csprof_state_t *state, mcontext_t *context)
{
    CONTEXT *ctx = (CONTEXT *) context;

    if(csprof_swizzle_patch_is_address(state)) {
        void **ret = state->swizzle_patch;

        /* the expected case is that the patch location contains the
           return address */
        if(*ret == CSPROF_TRAMPOLINE_ADDRESS) {
            /* we need to remove the patch from that location */
            *ret = state->swizzle_return;
            return;
        }
        /* the patch location may also contain the patched-out address,
           due to double-signaling without progress, or that we didn't
           make enough progress to store the register into the stack. */
        if(*ret == state->swizzle_return && *ret != CSPROF_TRAMPOLINE_ADDRESS) {
            /* ensure the return address register does not also contain
               the trampoline */
            DBGMSG_PUB(1, "Unable to unswizzle stack trampoline!");
            return;
        }
        /* hmmm, this is the weird error-handling case.  come back to this */
        if(*ret != state->swizzle_return && *ret != CSPROF_TRAMPOLINE_ADDRESS) {
            DBGMSG_PUB(1, "Unable to unswizzle stack trampoline!");
            /* probably */
            csprof_state_flag_set(state, CSPROF_TAIL_CALL);

            return;
        }
    }
    else if(csprof_swizzle_patch_is_register(state)) {
        unsigned int reg = (unsigned int)state->swizzle_patch;

        if(ctx->sc_regs[reg] == CSPROF_TRAMPOLINE_ADDRESS) {
            ctx->sc_regs[reg] = state->swizzle_return;
        }
    }
}

void
csprof_insert_trampoline(csprof_state_t *state, struct lox *l,
                         mcontext_t *context)
{
    CONTEXT *ctx = (CONTEXT *) context;
    void **p;

    /* swizzle the current location */
    if(l->current.type == REGISTER) {
        int reg = l->current.location.reg;

        p = (void **)&ctx->sc_regs[reg];
    }
    else {
        void **address = l->current.location.address;

        p = address;
    }

    /* don't modify if the trampoline is already there */
    if(*p != CSPROF_TRAMPOLINE_ADDRESS) {
        state->swizzle_return = *p;
        *p = CSPROF_TRAMPOLINE_ADDRESS;
    }
    else {
        DBGMSG_PUB(1, "Not swizzling %p because trampoline is there", p);
        if(l->current.type == REGISTER) {
            DBGMSG_PUB(1, "Register %d", l->current.location.reg);
        }
    }
#if 0
    /* record the stored location */
    if(l->stored.type == REGISTER) {
        state->swizzle_patch = (void **)l->stored.location.reg;
    }
    else {
        state->swizzle_patch = l->stored.location.address;
    }
#endif
}


/* support routines */

/* As of Tru64 UNIX V5.1 there may be many "linked" RPDs, describing a
     single procedure.  To detect whether two PCs fall in the same
     procedure, we must find the primary (distinguished) RPD for the
     procedure.  This one will have a RETURN_ADDRESS field of zero.
     Prior to V5.1, this linking was not used, and *ALL* RPDs had a zero
     RETURN_ADDRESS field. */
/* from email from Paul Drongowski <paul.drongowski@hp.com> */
extern long __exc_crd_list_head;

void
csprof_get_primary_rpd_and_crd(void *pc, pdsc_rpd **rpd, pdsc_crd **func_entry)
{
    int return_address = 1;
    pdsc_crd *crd;

    do {
        crd = exc_remote_lookup_function_entry(NULL, NULL, (exc_address)pc,
                                               (exc_address)&__exc_crd_list_head,
                                               func_entry, NULL);
        *rpd = PDSC_CRD_PRPD(crd);
        if (!*rpd) {
            /* The routine is a null frame procedure, which doesn't have
               an RPD.  Our caller will have to know how to handle it. */
            return;
       }

#ifdef PDSC_RPD_RETURN_ADDRESS_FIELD
       return_address = PDSC_RPD_RETURN_ADDRESS_FIELD(*rpd);
       if (return_address)
           pc = PDSC_RPD_RETURN_ADDRESS(*func_entry, *rpd);
#else
       return_address = 0;
#endif
    } while(return_address);
}

void
csprof_get_primary_rpd(void *pc, pdsc_rpd **rpd)
{
    pdsc_crd *crd;

    csprof_get_primary_rpd_and_crd(pc, rpd, &crd);
}

/* want the real RPD describing the region in which `pc' falls */
void
csprof_get_pc_rpd_and_crd(void *pc, pdsc_rpd **rpd, pdsc_crd **func_entry)
{
    pdsc_crd *crd;

    crd = exc_remote_lookup_function_entry(NULL, NULL, (exc_address)pc,
                                           (exc_address)&__exc_crd_list_head,
                                           func_entry, NULL);
    *rpd = PDSC_CRD_PRPD(crd);
}

/* functionality needed for swizzling things */

/* not static because it's also needed for backtracing */
unsigned int *
csprof_find_stqra_inst(unsigned int *begin, unsigned int *end)
{
    unsigned int *i;

    for(i = begin; i < end; ++i) {
        unsigned int inst = *i;

        if(csprof_inst_is_stqra(inst)) {
            return i;
        }
    }

    return end;
}

static unsigned int *
csprof_find_bis_inst(unsigned int *begin, unsigned int *end,
                     /* register numbers */
                     unsigned int from, unsigned int to)
{
    unsigned int *i;

    for(i = begin; i<end; ++i) {
        unsigned int inst = *i;

        if(csprof_inst_is_bis(inst, from, to)) {
            return i;
        }
    }

    return end;
}

#if TRAMPOLINE_SAVE_ALL_INT_REGISTERS
static int
calc_offset_for_register(int reg)
{
    /* the upper bound here is calculated from the registers saved in
       csprof_tramp.s.  don't forget to keep them in sync */
    if((reg > 27) || (reg < 0)) {
        DIE("Invalid value for register: %d", __FILE__, __LINE__, reg);
    }

    /* have different cases because $26 is stored in a funny place.
       XXX: bare 26 here */
    if(reg < 26) {
        /* hop over $26 and increment to the proper place */
        return 1 + reg;
    }
    else if(reg == 26) {
        /* $26 is stored at the very beginning */
        return 0;
    }
    else {
        return reg;
    }
}
#else
/* registers are saved in a different pattern if we're not saving
   all of them */
static int
calc_offset_for_register(int reg)
{
    /* $26 is stored at the very beginning */
    if(reg == 26) {
        return 0;
    }
    else if (0 <= reg && reg <= 8) {
        /* hop over $26 and increment to the proper place */
        return 1 + reg;
    }
    else if(reg == 15) {
        /* hop hop hop */
        return 10;
    }
    else if(22 <= reg && reg <= 25) {
        /* hop over $26 and the first batch of temp registers */
        return (reg - 22) + 11;
    }
    else {
        DIE("Invalid value for register: %d", __FILE__, __LINE__, reg);
    }
}
#endif

static void
find_ra_in_leaf_procedure(csprof_state_t *state, struct lox *l,
                          CONTEXT *ctx, pdsc_rpd *rpd, pdsc_crd *crd)
{
    void **return_address = NULL;
    void *p = NULL;
    int save_reg = PDSC_RPD_SAVE_RA(rpd);

    l->current.type = l->stored.type = REGISTER;

    if(rpd == NULL) {
        /* null frame procedure */
        l->current.location.reg = l->stored.location.reg = 26;
    }
    else {
        /* register frame procedure

           it's possible that the sample event has placed us in a
           position where we're *before* the

           bis zero, ra, <reg>

           instruction has moved the register where we think it was
           going to go.  we'll have to check for that case.  this is
           very similar to the hoops we need to jump through for the
           stack frame procdure case. */
        /* don't have to do magic if the entry RA is the same as the
           save RA.  this will happen if the procedure actually needs
           to allocate stack space to store things but doesn't need to
           save the return address. */
        int entry_reg = PDSC_RPD_ENTRY_RA(rpd);

        if(save_reg == entry_reg) {
            l->current.location.reg = l->stored.location.reg = save_reg;
        }
        else {
            /* save register is not equal to the entry register.  there
               must be a copy somewhere */
            void *pc = ctx->sc_pc;
            void *func_begin =
                PDSC_CRD_BEGIN_ADDRESS(exc_lookup_function_table(pc), crd);
            unsigned int *func_body = 
                (unsigned int *)VPTR_ADD(func_begin, PDSC_RPD_ENTRY_LENGTH(rpd));

            /* CRDs matter for register frame procedures, too */
            if(PDSC_CRD_TYPE_STANDARD(crd)) {
                /* test for prologue possibilities */
                if(pc < func_body) {
                    /* it usually seems that the bis instruction is at the end
                       of the three-instruction prologue, but this is a safety
                       check.  could be replaced if we knew it was always going
                       to be a three-instruction prologue with the bis at the
                       end */
                    unsigned int *bis_inst_loc =
                        csprof_find_bis_inst((unsigned int *)func_begin, func_body,
                                             entry_reg, save_reg);
                    if(bis_inst_loc >= (unsigned int *) pc) {
                        DBGMSG_PUB(1, "(leaf/handler) no bis to save, entry: %d", entry_reg);
                        l->current.location.reg = entry_reg;
                        l->stored.location.reg = save_reg;
                    }
                    else {
                        /* FIXME: I think it might be a bit more complicated
                           than what this case currently does */
                        l->current.location.reg = l->stored.location.reg = save_reg;
                    }
                }
                else {
                    goto REG_FRAME_BODY;
                }
            }
            else {
            REG_FRAME_BODY:
                /* we are not quite out of the woods yet.  apparently
                   when Digital's docs say that everybody returns through
                   the reserved instruction sequence

                     ret zero, (ra), 1

                   they mean everybody, even if it means an extra instruction
                   for register frame procedures to copy the saved register
                   contents back into $ra.

                   in other words, register frame procedures have an epilogue,
                   too, where they might release stack space they have
                   allocated, if any, move the return address from the save
                   register to $ra, and *then* return.  we have to check for
                   that case, too... */
                /* madness.  I hate Digital's register frame procdures */
                l->current.location.reg = l->stored.location.reg = save_reg;
                return_address = (void **)&(ctx->sc_regs[save_reg]);
                p = *return_address;

                return_address = (void **)&(ctx->sc_regs[entry_reg]);
                if(*return_address == p) {
                    *return_address = CSPROF_TRAMPOLINE_ADDRESS;
                }
            }

            return;
        }
    }

    /* FIXME: there was some other goop running around in the original
       csprof code at this point.  Examine said goop to see if anything
       needs to be ported over. */
    return_address = (void **)&(ctx->sc_regs[l->current.location.reg]);

    if(ctx->sc_regs[26] == *return_address
       && l->current.location.reg != 26) {
        ctx->sc_regs[26] = CSPROF_TRAMPOLINE_ADDRESS;
    }

    DBGMSG_PUB(1, "(leaf/handler) return reg: %d, val: %#lx",
               save_reg, *return_address);

#if 0
    /* in the handler case, ctx is modifiable, since the OS will restore
       all of the register values in it for us when the signal handler
       returns.  So muck with it directly. */
    /* save and swizzle */
    state->swizzle_patch = save_reg;
    state->extra_state = ctx->sc_pc;
    /* avoid infinite loops caused by register frame procedures
       tail-calling register frame procedures */
    if(*return_addr != CSPROF_TRAMPOLINE_ADDRESS) {
        state->swizzle_return = *return_addr;

        *return_addr = CSPROF_TRAMPOLINE_ADDRESS;

        /* swizzle the return address register too, for much the same
           reason as it gets done for stack frame procedures: procedures
           are not required to restore the return address register from
           the save register if there is no need to */
        if(ctx->sc_regs[26] == state->swizzle_return) {
            ctx->sc_regs[26] = CSPROF_TRAMPOLINE_ADDRESS;
        }
    }
#endif
}

static void
find_ra_in_stack_procedure(csprof_state_t *state, struct lox *l, CONTEXT *ctx,
                           pdsc_rpd *rpd, pdsc_crd *crd)
{
    unsigned int rsa_offset = PDSC_RPD_RSA_OFFSET(rpd);
    unsigned int frame_register;
    unsigned int sp_set_offset;
    /* which register holds the return address on function entry */
    unsigned int entry_ra_register;
    int check_for_epilogue = 1;

    void *pc = ctx->sc_pc;
    /* the address of the start of the function */
    void *func_begin =
        PDSC_CRD_BEGIN_ADDRESS(exc_lookup_function_table(pc),crd);
    unsigned int prologue_len = PDSC_RPD_ENTRY_LENGTH(rpd);
    void *func_body_begin = VPTR_ADD(func_begin, prologue_len);

    void **return_address_location;
    
    /* common information for several of the cases below */
    if(PDSC_RPD_BASE_REG_IS_FP(rpd)) {
        frame_register = FP_REG;
    }
    else {
        frame_register = SP_REG;
    }
    /* needed in several cases */
    entry_ra_register = PDSC_RPD_ENTRY_RA(rpd);

    /* the trick which saves us some time and produces more accurate
       results is that the crd sometimes tells us the information we
       need to know.  according to <pdsc.h>, the crd can indicate five
       different kinds of ranges.  these ranges are indicated by three
       flags in the crd, s/t/p, whose values are listed with their
       corresponding kind of range below:

       + standard (0/0/0) - prologue and body
       + context (0/0/1) - just body, no prologue
       + data (0/1/0) - data residing in the text area (jump tables?)
       + non_context (0/1/1) - "not in a routine context" and no
         stack management is necessary here (probably due to tail
         merging of procedures, perhaps?)
       + non_context_stack (1/0/1) - non-context region, but the stack
       has been allocated (e.g. exception handlers)

       I didn't think these mattered for the longest time, but
       apparently the compiler puts them to good use and they are needed
       on the SPEC benchmarks, for instance, to ensure the trampoline
       gets positioned in the correct location. */
    if(PDSC_CRD_TYPE_STANDARD(crd)) {
        /* must do full determination, including prologue */
        void *sp_set_inst_loc = VPTR_ADD(func_begin, PDSC_RPD_SP_SET(rpd));

        DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "func begin: %#lx, prologue_len: %d",
                   func_begin, prologue_len);

        if(pc < func_body_begin) {
            /* in the prologue */

            /* ick */
            rsa_offset = PDSC_RPD_RSA_OFFSET(rpd);
            if(pc <= sp_set_inst_loc) {
                /* hmmm...we haven't set the stack pointer yet.
                   find out what it's going to be set to */
                DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack/before lda $sp)");

                unsigned int sp_set_inst = *((unsigned int *)sp_set_inst_loc); 

                if(((sp_set_inst >> 21) & 0x1f) == 30) {
                    short sp_move_amount = PDSC_RPD_SIZE(rpd);

                    unsigned long new_sp = ctx->sc_regs[30] - sp_move_amount;
                    unsigned long rsa_area_start = new_sp + rsa_offset;

                    DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack/before) sp_move_amt: %d", sp_move_amount);
                    DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack/before) new sp: %#lx", new_sp);
                    DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack/before) return addr: %#lx",
                               ctx->sc_regs[entry_ra_register]);

                    l->current.type = REGISTER;
                    l->current.location.reg = entry_ra_register;
                    l->stored.type = ADDRESS;
                    l->stored.location.address = (void **) rsa_area_start;

                    return;
                }
                else {
                    goto BODY_PROCEDURE;
                }
            }
            else {
                /* FIXME: entry ra is not always 26, necessarily */
                /* if we have executed the $sp set instruction,
                   then we need to find out whether or not we've
                   stored the ra in the stack frame.  this knowledge
                   lets us do one of two things:
               
                   1) if we haven't stored $ra, then calculate the
                   location at which it would be stored and then
                   modify the appropriate register in the context
               
                   2) if we have stored $ra, then simply calculate
                   the address where it is stored and swizzle it
                   directly */
                unsigned int *inst_scanner = (unsigned int*)pc;
                /* where the action actually happens */
                unsigned int *func_body = (unsigned int *)func_body_begin;

                unsigned int *stqra_inst_loc;

                DBGMSG_PUB(1, "scan: %#lx, body: %#lx", inst_scanner, func_body);

                stqra_inst_loc = csprof_find_stqra_inst(inst_scanner,
                                                        func_body);
                if(stqra_inst_loc == func_body) {
                    /* Digital's compiler--particularly their C++ compiler--
                       likes to assign small pieces of code an RPD which
                       actually has nothing whatsoever in common with the
                       behavior of the code in question.  for example, a short
                       piece of epilogue code between the reloading of the
                       stack pointer and the subsequent return will be given
                       an RPD which closely matches that of the main function.
                       in particular, it has things like a prologue length and
                       so forth.

                       what this means in practice is we might have gotten here,
                       but the code has no intention of storing $ra anywhere,
                       nor has it already been stored.  it's probably waiting
                       for an return instruction to jump back to its caller...
                       which will appropriately screw us over at a later point.

                       so test for this.  thankfully, the "entry point" of
                       the epilogue code snippet is correctly reported.  the
                       following idea seems to work well: check the entire body
                       of the snippet (should be small) for a stqra instruction.
                       if no such instruction is found, then assume the return
                       address register is the place to look for the appropriate
                       return address.  otherwise, we're in a "real" stack frame
                       procedure */
                    unsigned int *stqra_inst_loc_checker = csprof_find_stqra_inst(func_begin,
                                                                                  func_body);
                    if(stqra_inst_loc_checker == stqra_inst_loc) {
                        /* fake stack frame procedure */
                        DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "Fake stack frame procedure detected");

                        l->current.type = l->stored.type = REGISTER;
                        l->current.location.reg = l->stored.location.reg = 26;

                        return;
                    }
                    else {
                        /* we have stored the return address on the stack.
                           swizzle as "normal."  except that we don't know
                           whether or not we've set the frame register
                           properly...but we *do* know that the stack pointer
                           is properly positioned, so use that */
                        frame_register = SP_REG;

                        DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack/after stq $ra)");

                        check_for_epilogue = 0;

                        /* Dijkstra would kill me */
                        goto MAIN_BODY;
                    }
                }
                else {
                    /* `stqra_inst_loc' now points to the ra storing address.
                       figure out what the offset is and then swizzle in
                       the actual context, since the code has yet to store
                       the return address in the stack */
                    /* FIXME: could we do this with an int, please? */
                    short store_offset = (short)(*stqra_inst_loc & 0x0000ffff);

                    DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "stqra offset: %d", store_offset);

                    /* where the return address is going */
                    l->current.type = REGISTER;
                    l->current.location.reg = entry_ra_register;
                    l->stored.type = ADDRESS;
                    l->stored.location.address = (void **)(ctx->sc_regs[SP_REG] + store_offset);

                    return;
                }
            }
        }
        else {
            goto BODY_PROCEDURE;
        }
    }
    else if(PDSC_CRD_TYPE_CONTEXT(crd)) {
        /* swizzle it like it was a body procedure */
        DBGMSG_PUB(1, "context crd");

        goto BODY_PROCEDURE;
    }
    else if(PDSC_CRD_TYPE_DATA(crd)) {
        /* should never see one of these */
        DBGMSG_PUB(1, "data crd");

        DIE("DATA code range descriptor seen", __FILE__, __LINE__);
    }
    else if(PDSC_CRD_TYPE_NON_CONTEXT(crd)) {
        /* the code range descriptors lie to us sometimes.  this
           type of code range can actually cover a whole section of
           code like the following:

           ldq ra, 0(sp)
           ldq s0, 8(sp)
           ...
           lda sp, 64(sp)
           ret zero, ra, 1

           pay attention to the lies here */
        void **pcptr = (void **)(ctx->sc_pc);
        unsigned int inst = *((unsigned int *)pcptr);

        DBGMSG_PUB(1, "non-context crd, entry reg: %d = %#lx",
                   entry_ra_register, ctx->sc_regs[entry_ra_register]);

        if(inst >> 16 == 0xa75e) { /* ldq ra, ...(sp) */
            /* it cheats, precious! */
            check_for_epilogue = 0;
            goto MAIN_BODY;
        }
        else {
            l->current.type = l->stored.type = REGISTER;
            l->current.location.reg = entry_ra_register;
            /* record it as for a register frame procedure */
            l->stored.location.reg = 26;

            return;
        }
    }
    else if(PDSC_CRD_TYPE_NON_CONTEXT_STACK(crd)) {
        /* swizzle it like it was a body procedure */
        DBGMSG_PUB(1, "non-context stack crd");

        goto BODY_PROCEDURE;
    }

 BODY_PROCEDURE:
    {
        /* in the body of the function...or the epilogue.  we have to
           check if the stack pointer has already been reset, because we
           index off of the stack pointer to find the correct return
           address. */
        if(check_for_epilogue) {
            if(csprof_state_flag_isset(state, CSPROF_EPILOGUE_RA_RELOADED | CSPROF_EPILOGUE_SP_RESET)) {
                void **stack_ra_loc;

                DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "epilogue");

                /* stack pointer has been reset, so shuffle it back to the
                   proper location...not that this *really* matters, since
                   soon enough we should be returning from the function before
                   another sample event is received */
                l->current.type = l->stored.type = REGISTER;
                l->current.location.reg = l->stored.location.reg = 26;

                /* blech */
                if(csprof_state_flag_isset(state, CSPROF_EPILOGUE_RA_RELOADED)) {
                    stack_ra_loc = (void **)(ctx->sc_regs[frame_register] + rsa_offset);
                }
                else if(csprof_state_flag_isset(state, CSPROF_EPILOGUE_SP_RESET)) {
                    stack_ra_loc = (void **)(ctx->sc_regs[30] + rsa_offset - PDSC_RPD_SIZE(rpd));
                }

                DBGMSG_PUB(1, "Stack RA loc: %p => %p", stack_ra_loc, *stack_ra_loc);

                if(*stack_ra_loc == ctx->sc_regs[26]) {
                    *stack_ra_loc = CSPROF_TRAMPOLINE_ADDRESS;
                }
            }
            else {
                goto MAIN_BODY;
            }
        }
        else {
            MAIN_BODY:
            /* find the location of the return address on the stack */
            return_address_location =
                (void **)(ctx->sc_regs[frame_register] + rsa_offset);

            l->current.type = l->stored.type = ADDRESS;
            l->current.location.address = l->stored.location.address = return_address_location;

            if(*return_address_location == ctx->sc_regs[entry_ra_register]) {
                /* swizzle the entry register, just to make sure everything
                   works out OK for stupid optimized-beyond-sanity
                   procedures which save the return address register to
                   the stack, but don't bother to restore it along some
                   paths through the procedure */
                /* also check to ensure we're not swizzling $ra out from
                   underneath an instruction which uses $ra (particularly
                   an instruction which uses $ra to restore the global
                   pointer register--if we corrupt that, everything falls
                   apart!).  if an
                   instruction uses it, it's quite likely we're not running
                   along a path which fails to restore $ra */
                unsigned int *pcptr = pc;
                unsigned int inst = *pcptr;

                /* thanks to Alpha's extremely regular instruction set... */
                if((csprof_inst_is_return(inst))
                   || (((inst >> 16) & 0x1f) != 26) /* reads from $ra */
                   || (((inst >> 28) & 0xf) == 0x5)) { /* fp instruction */
                    DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack) swizzling register %d = %#lx",
                               entry_ra_register, ctx->sc_regs[entry_ra_register]);

                    ctx->sc_regs[entry_ra_register] = CSPROF_TRAMPOLINE_ADDRESS;
                }
            }

            if(frame_register != 30) {
                DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack) Frame register: %d", frame_register);
            }
            DBGMSG_PUB(CSPROF_DBG_STACK_TRAMPOLINE, "(stack) raloc %#lx = %#lx | $ra %#lx",
                       return_address_location,
                       *return_address_location,
                       ctx->sc_regs[26]);
        }
    }
}
