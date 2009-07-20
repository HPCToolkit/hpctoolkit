// -*-Mode: C++;-*- // technically C99
// $Id$

/* metrics.c -- informing the system about what metrics to record */
#if defined(__ia64__) && defined(__linux__)
#include <libunwind.h>
#endif

#include "epoch.h"
#include "metrics.h"
#include "state.h"
#include "monitor.h"

#include <messages/messages.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

/* total number of metrics we can track simultaneously */
static int csprof_max_metrics = 0;
/* information about tracked metrics */

static metric_tbl_t metric_data;

/* setting the total number of metrics to track */
static int has_set_max_metrics = 0;

int
csprof_get_max_metrics()
{
  return csprof_max_metrics;
}

int
csprof_set_max_metrics(int max_metrics)
{
  TMSG(METRICS,"Set max metrics to %d", max_metrics);
  if(has_set_max_metrics) {
    TMSG(METRICS,"Set max metrics called AGAIN with %d", max_metrics);
    return -1;
  }
  else {
    TMSG(METRICS,"Set 'max_set' flag. Initialize metric data");
    has_set_max_metrics = 1;
    csprof_max_metrics = max_metrics;

    TMSG(METRICS," set_max_metrics");
    metric_data.lst = csprof_malloc(max_metrics * sizeof(metric_desc_t));
    return max_metrics;
  }
}

metric_tbl_t*
hpcrun_get_metric_data(void)
{
  return &metric_data;
}

int
csprof_num_recorded_metrics(void)
{
  return metric_data.len;
}

/* providing hooks for the user to define their own metrics */

int
csprof_new_metric()
{
  if(metric_data.len >= csprof_max_metrics) {
    EMSG("Unable to allocate any more metrics");
    return -1;
  }
  else {
    int the_metric = metric_data.len;
    metric_data.len++;
    return the_metric;
  }
}

void
csprof_set_metric_info_and_period(int metric_id, char *name,
				  uint64_t flags, size_t period)
{
  TMSG(METRICS,"id = %d, name = %s, flags = %lx, period = %d", metric_id, name,flags, period);
  if(metric_id >= metric_data.len) {
    EMSG("Metric id `%d' is not a defined metric", metric_id);
  }
  if(name == NULL) {
    EMSG("Must supply a name for metric `%d'", metric_id);
    monitor_real_abort();
  }
  { 
    metric_desc_t *metric = &(metric_data.lst[metric_id]);
    metric->name = name;
    metric->period = period;
    metric->flags = flags;
  }
}

void
csprof_set_metric_info(int metric_id, char *name, uint64_t flags)
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
