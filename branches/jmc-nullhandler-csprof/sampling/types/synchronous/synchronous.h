#ifndef CSPROF_SYNCHRONOUS_H
#define CSPROF_SYNCHRONOUS_H

/* csprof_fetch_stack_pointer takes a `void **' to force procedures in
   which it is used to require a stack frame.  this gets around several
   problems, notably the problem of return addresses being in registers. */

#if defined(OSF) && defined(__DECC)

static inline void
csprof_fetch_stack_pointer(void **stack_pointer_loc)
{
    asm("stq sp, 0(v0)", stack_pointer_loc);
}

/* is this procedure general enough to be moved outside #if? */
static inline void
csprof_take_synchronous_sample(unsigned int sample_count)
{
    void *ra_loc;
    csprof_state_t *state = csprof_get_state();

    csprof_undo_swizzled_data(state, NULL);
    csprof_fetch_stack_pointer(&ra_loc);
    csprof_take_sample(sample_count);
    csprof_swizzle_location(state, ra_loc);
}

#endif

#endif
