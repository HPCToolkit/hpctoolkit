/* metrics.c -- informing the system about what metrics to record */
#if defined(__ia64__) && defined(__linux__)
#include <libunwind.h>
#endif

#include "general.h"
#include "mem.h"
#include "epoch.h"
#include "metrics.h"
#include "state.h"

#include "hpcfile_csprof.h"

/* total number of metrics we can track simultaneously */
static int csprof_max_metrics = 0;
/* information about tracked metrics */
hpcfile_csprof_data_t metric_data;


/* setting the total number of metrics to track */
static int has_set_max_metrics = 0;

int
csprof_get_max_metrics()
{
  return csprof_max_metrics;
}

int csprof_set_max_metrics(int max_metrics){
  if(has_set_max_metrics) {
    ERRMSG("Unable to set max_metrics multiple times", __FILE__, __LINE__);
    return -1;
  }
  else {
    has_set_max_metrics = 1;
    csprof_max_metrics = max_metrics;
    hpcfile_csprof_data__init(&metric_data);
    metric_data.target = "--FIXME--target_name";
    metric_data.metrics = csprof_malloc(max_metrics * sizeof(hpcfile_csprof_metric_t));
    return max_metrics;
  }
}

hpcfile_csprof_data_t *csprof_get_metric_data(){
  return &metric_data;
}

int csprof_num_recorded_metrics(){
  return metric_data.num_metrics;
}


/* providing hooks for the user to define their own metrics */

int
csprof_new_metric()
{
  if(metric_data.num_metrics >= csprof_max_metrics) {
    ERRMSG("Unable to allocate any more metrics", __FILE__, __LINE__);
    return -1;
  }
  else {
    int the_metric = metric_data.num_metrics;
    metric_data.num_metrics++;
    return the_metric;
  }
}

void
csprof_set_metric_info_and_period(int metric_id, char *name,
				  int flags, size_t period)
{
  if(metric_id >= metric_data.num_metrics) {
    ERRMSG("Metric id `%d' is not a defined metric",
           __FILE__, __LINE__, metric_id);
  }
  if(name == NULL) {
    DIE("Must supply a name for metric `%d'", __FILE__, __LINE__, metric_id);
  }
  { 
    hpcfile_csprof_metric_t *metric = &metric_data.metrics[metric_id];
    metric->metric_name = name;
    metric->sample_period = period;
    metric->metric_flags = flags;
  }
}

void
csprof_set_metric_info(int metric_id, char *name, int flags)
{
  csprof_set_metric_info_and_period(metric_id, name, flags, 1);
}

#if 0
void
csprof_record_metric(int metric_id, size_t value)
{

#ifdef NO
  /* step out of c_r_m_w_u and c_r_m */
  csprof_record_metric_with_unwind(metric_id, value, 2);
#endif
  /** Try it with 1 **/
  csprof_record_metric_with_unwind(metric_id, value, 1);
}

#endif
