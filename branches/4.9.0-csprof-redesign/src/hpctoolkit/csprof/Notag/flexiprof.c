/* flexiprof: arbitrary application-driven metric recording */

#include "structs.h"
#include "metrics.h"

static int can_record_metrics = 0;

void
csprof_driver_init(csprof_state_t *state, csprof_options_t *options)
{
  /* pull MAX_METRIC info out of `options' */
  can_record_metrics = 1;

  csprof_set_max_metrics(options->max_metrics);
}

void
csprof_driver_fini(csprof_state_t *state, csprof_options_t *options)
{
  can_record_metrics = 0;
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
