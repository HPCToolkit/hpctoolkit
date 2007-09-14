/* a driver for programs that are going to manually call into the library */

#include <libunwind.h>

#include "state.h"
#include "structs.h"

void
csprof_record_sample(unsigned long amount)
{
  csprof_state_t *state = csprof_get_state();
  unw_context_t ctx;
  unw_cursor_t frame;

  if(state != NULL) {
    /* force insertion from the root */
    state->treenode = NULL;
    state->bufstk = state->bufend;
    state = csprof_check_for_new_epoch(state);

    /* FIXME: error checking */
    unw_get_context(&ctx);
    unw_init_local(&frame, &ctx);
    unw_step(&frame);		/* step out into our caller */

    csprof_sample_callstack_from_frame(state, amount, &frame);
  }
}

void
csprof_driver_init(csprof_state_t *state, csprof_options_t *options)
{
}

void
csprof_driver_fini(csprof_state_t *state, csprof_options_t *options)
{
}

#ifdef CSPROF_THREADS
void
csprof_driver_thread_init(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_thread_fini(csprof_state_t *state)
{
    /* no support required */
}
#endif

void
csprof_driver_suspend(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_resume(csprof_state_t *state)
{
    /* no support required */
}

