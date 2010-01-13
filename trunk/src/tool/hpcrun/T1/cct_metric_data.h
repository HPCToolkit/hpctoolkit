#ifndef CCT_METRIC_DATA_H
#define CCT_METRIC_DATA_H

#include <stdint.h>

typedef union cct_metric_data_t {
  intptr_t  i;
  uintptr_t u;
  double    d;
  void*     a;
} cct_metric_data_t;

#define metric_bin_fn_s(N) cct_metric_data_t N(cct_metric_data_t v1, cct_metric_data_t v2)

typedef metric_bin_fn_s((*metric_bin_fn));

#endif // CCT_METRIC_DATA_H
