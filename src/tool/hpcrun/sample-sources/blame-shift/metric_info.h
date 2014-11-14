#ifndef __metric_info_h__
#define __metric_info_h__

static inline int
metric_is_timebase(int metric_id, int *period)
{
  metric_desc_t * metric_desc = hpcrun_id2metric(metric_id);

  if (metric_desc == NULL) return 0;

  *period = metric_desc->period;
 
  // temporal blame shifting requires a time or cycles metric
  return (metric_desc->properties.time | metric_desc->properties.cycles);
}



#endif // __metric_info_h__
