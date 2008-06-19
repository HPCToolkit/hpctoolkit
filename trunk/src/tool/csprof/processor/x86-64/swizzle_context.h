#ifndef SWIZZLE_CONTEXT_H
#define SWIZZLE_CONTEXT_H
void
csprof_undo_swizzled_data(csprof_state_t *state, void *ctx);

void
csprof_swizzle_with_context(csprof_state_t *state, void *ctx);
#endif
