#ifndef _HPCTOOLKIT_CUPTI_ACTIVITY_API_H_
#define _HPCTOOLKIT_CUPTI_ACTIVITY_API_H_

#include <cupti.h>

extern void
cupti_activity_handle
(
 CUpti_Activity *activity
);


extern bool
cupti_advance_buffer_cursor
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity *current,
  CUpti_Activity **next
);

#endif
