#ifndef cupti_range_h
#define cupti_range_h

#include <stdint.h>

#define CUPTI_RANGE_DEFAULT_INTERVAL 1
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD 100
#define CUPTI_RANGE_STR(x) #x
#define CUPTI_RANGE_DEFAULT_INTERVAL_STR CUPTI_RANGE_STR(CUPTI_RANGE_DEFAULT_INTERVAL)
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD_STR CUPTI_RANGE_STR(CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD)

typedef enum cupti_range_mode {
  CUPTI_RANGE_MODE_NONE,
  CUPTI_RANGE_MODE_EVEN,
  CUPTI_RANGE_MODE_CONTEXT_SENSITIVE,
  CUPTI_RANGE_MODE_COUNT
} cupti_range_mode_t;

void
cupti_range_config
(
 const char *mode_str,
 int interval,
 int sampling_period
);

cupti_range_mode_t
cupti_range_mode_get
(
 void
);

uint32_t
cupti_range_interval_get
(
 void
);

uint32_t
cupti_range_sampling_period_get
(
 void
);

#endif
