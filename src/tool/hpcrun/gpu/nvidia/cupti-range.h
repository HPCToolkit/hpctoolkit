#ifndef cupti_range_h
#define cupti_range_h

#include <stdint.h>

#define CUPTI_RANGE_DEFAULT_INTERVAL 1
#define CUPTI_RANGE_DEFAULT_INTERVAL_STR "1"
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD 1
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD_STR "1"

#define CUPTI_RANGE_THREAD_RETAIN_RANGE 100
#define CUPTI_RANGE_THREAD_RETAIN_RANGE_STR "100"

typedef enum cupti_range_mode {
  CUPTI_RANGE_MODE_NONE = 0,
  CUPTI_RANGE_MODE_SERIAL = 1,
  CUPTI_RANGE_MODE_EVEN = 2,
  CUPTI_RANGE_MODE_CONTEXT_SENSITIVE = 3,
  CUPTI_RANGE_MODE_COUNT = 4
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

// Called at process finish callback to get back pc samples for the last range
void
cupti_range_last
(
 void
);

#endif
