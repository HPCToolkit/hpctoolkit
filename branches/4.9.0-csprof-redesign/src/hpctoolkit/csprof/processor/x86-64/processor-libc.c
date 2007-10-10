#include <setjmp.h>

#include "interface.h"

void
arch_libc_init()
{
    /* nothing to do ATM; exception handling stuff goes here */
}

/* !!!! GIANT HACK  !!!!!! 
   Don't know what rtn address and sp are for this arch, so
   better not call setjmp or longjmp in user code.
   Set to arbitrary value now
*/
#ifdef JB_PC
# undef JB_PC
#endif
#ifdef JB_RSP
# undef JB_RSP
#endif

#define JB_PC  0
#define JB_RSP 1

/* !!!! END GIANT HACK !!!!! */

#ifdef CSPROF_TRAMPOLINE_BACKEND

void
csprof_nlx_to_context(csprof_state_t *state, jmp_buf env)
{
    struct __jmp_buf_tag *xenv = &env[0];
    __jmp_buf *zenv = &xenv->__jmpbuf;
    void *pc;
    void *sp;
    void *return_address;

    pc = (void *)zenv[0][JB_PC];
    sp = (void *)zenv[0][JB_RSP];

    if(pc == CSPROF_TRAMPOLINE_ADDRESS) {
	pc = csprof_bt_top_ip(state);
        zenv[0][JB_PC] = (long int) csprof_bt_top_ip(state);
        *(state->swizzle_patch) = state->swizzle_return;
    }

    if(!csprof_state_has_empty_backtrace(state)) {
        if(sp < csprof_bt_top_sp(state)) {
            /* nothing to do here */
        }
        else if(sp == csprof_bt_top_sp(state)) {
            /* nothing to do here */
        }
        else {
            /* undo the current swizzled state */
            *(state->swizzle_patch) = state->swizzle_return;
            state->swizzle_patch = state->swizzle_return = 0;

            /* used to be '!=', but I think '>' is OK.  using '!=' means
	       we have to resort to unwinding to figure out the "proper"
	       $sp and that's a real pain. */
            while((csprof_bt_top_sp(state) > sp)
                  && !csprof_state_has_empty_backtrace(state)) {
                csprof_bt_pop(state);
            }

            csprof_swizzle_with_jmpbuf(state, env);
        }
    }
}

#endif
