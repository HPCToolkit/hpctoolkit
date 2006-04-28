#include <sys/types.h>
#include <ucontext.h>

#include "interface.h"
#include "general.h"

extern void *__libc_start_main;

void *
csprof_get_pc(void *context)
{
    mcontext_t *ctx = (mcontext_t *)context;

    return (void *)ctx->sc_ip;
}

int
csprof_find_return_address_for_context(csprof_state_t *state,
                                       struct lox *l, mcontext_t *ctx)
{
    return 0;
}

int
csprof_find_return_address_for_function(csprof_state_t *state,
                                        struct lox *l, void **stackptr)
{

#if 0
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
#endif

    return 0;
}

void
csprof_remove_trampoline(csprof_state_t *state, mcontext_t *context)
{
#if 0
    if(csprof_swizzle_patch_is_register(state)) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = state->swizzle_patch;

        *addr = state->swizzle_return;
    }
#else
    DIE("IA64 backend doesn't support trampolines", __FILE__, __LINE__);
#endif
}

void
csprof_insert_trampoline(csprof_state_t *state, struct lox *l, mcontext_t *context)
{
#if 0
    if(l->current.type == REGISTER) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = l->current.location.address;

	DBGMSG_PUB(8, "Inserting trampoline at %p = %p",
		   addr, *addr);

        state->swizzle_return = *addr;
        *addr = CSPROF_TRAMPOLINE_ADDRESS;
    }
#else
    DIE("IA64 backend doesn't support trampolines", __FILE__, __LINE__);
#endif
}
