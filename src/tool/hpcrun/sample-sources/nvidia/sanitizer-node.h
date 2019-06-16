#ifndef _HPCTOOLKIT_SANITIZER_NODE_H_
#define _HPCTOOLKIT_SANITIZER_NODE_H_

#include <cuda.h>

typedef enum {
  SANITIZER_ENTRY_TYPE_MEMORY = 1,
  SANITIZER_ENTRY_TYPE_COUNT = 2
} sanitizer_entry_type_t;

// generic entry
typedef struct sanitizer_node {
  struct sanitizer_node *next;
  void *entry;
  sanitizer_entry_type_t type;
} sanitizer_node_t;

// memory
typedef struct sanitizer_memory_record {
  uint64_t pc;
  uint32_t size;
  uint32_t flags;
  dim3 thread_ids;
  dim3 block_ids;
} sanitizer_memory_record_t;

typedef struct sanitizer_memory {
  uint32_t cur_index;
  uint32_t max_index;
  sanitizer_memory_record_t *records;
} sanitizer_memory_t;

#endif
