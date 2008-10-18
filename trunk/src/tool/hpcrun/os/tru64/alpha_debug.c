/* alpha_debug.c -- printing various debugging information */

void
csprof_print_context(CONTEXT *ctx)
{
    unsigned int i;

    ERRMSG("PC: %#lx", __FILE__, __LINE__, ctx->sc_pc);

    for(i=0; i<31; ++i) {
        ERRMSG("$r%d: %lx", __FILE__, __LINE__, i, ctx->sc_regs[i]);
    }
}

/* meant for those cases where we're about to die, but we need a little
   useful information before we bail.  `ctx' may be null, depending on
   where we die. */
void
csprof_print_state(csprof_state_t *state, void *context)
{
    CONTEXT *ctx = (CONTEXT *) context;

    /* try to print out a little information; debugger backtraces
       tend to be unhelpful because the trampoline interferes. */
    csprof_print_backtrace(state);
    ERRMSG("Trampoline was located at: %#lx -> %#lx", __FILE__, __LINE__,
           state->swizzle_patch, state->swizzle_return);
    if(ctx != NULL) {
        csprof_print_context(ctx);
    }

    ERRMSG("Extra data: %#lx", __FILE__, __LINE__, state->extra_state);
    ERRMSG("we %s been through a trampoline since the last signal",
           __FILE__, __LINE__,
           csprof_state_flag_isset(state, CSPROF_THRU_TRAMP) ? "have" : "have not");
}

