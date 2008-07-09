void
csprof_undo_swizzled_data(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    if(csprof_swizzle_patch_is_valid(state)) {
        csprof_remove_trampoline(state, ctx);
    }
#endif
}

void
csprof_swizzle_with_context(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    struct lox l;
    int proceed = csprof_find_return_address_for_context(state, &l, ctx);

    if(proceed) {
        /* record */
        if(l.stored.type == REGISTER) {
            state->swizzle_patch = (void **) l.stored.location.reg;
        }
        else {
            state->swizzle_patch = l.stored.location.address;
        }
        /* recording the return is machine-dependent */

        csprof_insert_trampoline(state, &l, ctx);
    }
#endif
}
