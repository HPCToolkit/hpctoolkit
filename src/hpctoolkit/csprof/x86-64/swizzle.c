#include <sys/ucontext.h>

#include "interface.h"
#include "general.h"

extern void *__libc_start_main;

void *
csprof_get_pc(void *context)
{
    mcontext_t *ctx = (mcontext_t *)context;

    return (void *)ctx->gregs[REG_RIP];
}

int
csprof_find_return_address_for_context(csprof_state_t *state,
                                       struct lox *l, mcontext_t *ctx)
{
#if 1
    /* the unwinding process has figured out the base pointer for us */
    void **rbp = (void **)state->extra_state;

    l->current.type = l->stored.type = ADDRESS;
    l->current.location.address = l->stored.location.address = rbp-1;
#else
    /* supremely easy on x86-64 */
    void **rbp = (void **)ctx->gregs[REG_RBP];

    l->current.type = l->stored.type = ADDRESS;
    l->current.location.address = l->stored.location.address = rbp+1;
#endif

    return 1;
}

int
csprof_find_return_address_for_function(csprof_state_t *state,
                                        struct lox *l, void **stackptr)
{
    void **rbp;

    l->current.type = l->stored.type = ADDRESS;

#if 1
    /* we can't rely on the stored-on-the-stack RBP to be accurate.  what
       we do know, however, is that we've already recorded the CFA for
       this frame during a previous unwind process (!).  so we poke around
       in our cached unwind stack to find what we need */
    /* FIXME: there may well be some issues with running off the stack
       here...? */
    csprof_frame_t *caller = state->bufstk;

    rbp = caller->sp - sizeof(void *);
#else
    /* the offset here is dependent upon how many registers are saved in
       tramp.s.  if you modify the prologue and epilogue of the trampoline
       there, you need to change this as well. */
    rbp = ((void **)*(stackptr + 13)) + 1;
#endif

    l->current.location.address = l->stored.location.address = rbp;

    /* we have to beware of swizzling the return from libc_start_main;
       we take the evil measure of guessing how long the procedure is
       going to be to figure out when the instruction pointer lies
       within libc_start_main.  this should be reasonably safe. */
    {
        void *libc_start_main_addr = (void **)&__libc_start_main;
        void *libc_start_main_end = VPTR_ADD(libc_start_main_addr, 240);
        void *ret = *rbp;

        return !((libc_start_main_addr <= ret) && (ret < libc_start_main_end));
    }
}

void
csprof_remove_trampoline(csprof_state_t *state, mcontext_t *context)
{
    if(csprof_swizzle_patch_is_register(state)) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = state->swizzle_patch;

	DBGMSG_PUB(1, "Removing trampoline at %p", addr);
        *addr = state->swizzle_return;
    }
}

void
csprof_insert_trampoline(csprof_state_t *state, struct lox *l, mcontext_t *context)
{
    if(l->current.type == REGISTER) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = l->current.location.address;

	DBGMSG_PUB(1, "Inserting trampoline at %p = %p",
		   addr, *addr);

        state->swizzle_return = *addr;
        *addr = CSPROF_TRAMPOLINE_ADDRESS;
    }
}
