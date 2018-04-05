#ifndef _HPCTOOLKIT_CUPTI_OMPT_API_H_
#define _HPCTOOLKIT_CUPTI_OMPT_API_H_

#include <cupti.h>

extern bool
cupti_ompt_buffer_cursor_advance
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity *current,
  CUpti_Activity **next
);


extern void
cupti_ompt_activity_process
(
 CUpti_Activity *activity
);


extern void
cupti_ompt_activity_flush
( 
);


#endif
