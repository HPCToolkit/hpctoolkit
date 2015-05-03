#include <inttypes.h>

typedef struct pfq_rwlock_node_s {
  struct pfq_rwlock_node_s *next;
  volatile uint32_t blocked;
} pfq_rwlock_node;

// align a variable at the start of a cache line
// CLA = Cache Line Aligned
#define CLA(x) x __attribute__((aligned(64)))

typedef struct {
  CLA(volatile int flag);
} pfq_rwlock_flag;

typedef struct pfq_rwlock_s {
  CLA(uint32_t rin);
  CLA(uint32_t rout);
  CLA(uint32_t last);
  pfq_rwlock_flag flag[2]; 
  CLA(pfq_rwlock_node *wtail);
  CLA(pfq_rwlock_node *whead);
} pfq_rwlock;
