#include "metrics.h"
#include "papi_pulse.h"
#include "structs.h"

extern void unw_init(void);

void
csprof_process_driver_init(csprof_state_t *state, csprof_options_t *opts)
{
    setup_segv();
    unw_init();
    {
      int metric_id;

      csprof_set_max_metrics(2);
      metric_id = csprof_new_metric(); /* weight */
      csprof_set_metric_info_and_period(metric_id, "# samples",
					CSPROF_METRIC_ASYNCHRONOUS,
					opts->sample_period);
      metric_id = csprof_new_metric(); /* calls */
      csprof_set_metric_info_and_period(metric_id, "# returns",
					CSPROF_METRIC_FLAGS_NIL, 1);
    }
    // FIXME: papi specific for now
    papi_pulse_init();
}

void csprof_driver_fini(void){
  papi_pulse_fini();
}
