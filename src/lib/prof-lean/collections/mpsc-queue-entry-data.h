#ifndef GENERIC_MPSC_QUEUE_ENTRY_DATA_H
#define GENERIC_MPSC_QUEUE_ENTRY_DATA_H 1

#include <stdatomic.h>



#define MPSC_QUEUE_ENTRY_DATA(ENTRY_TYPE)     \
struct {                                      \
  _Atomic(ENTRY_TYPE *) next;                 \
  void *allocator;                            \
}

#endif
