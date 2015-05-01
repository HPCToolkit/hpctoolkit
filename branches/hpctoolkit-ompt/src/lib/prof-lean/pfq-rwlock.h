#include <inttypes.h>

typedef struct pfq_rwlock_node_s {
  struct pfq_rwlock_node_s *next;
  uint32_t blocked;
} pfq_rwlock_node;

typedef struct pfq_rwlock_s {
  uint32_t rin;
  uint32_t rout;
  uint32_t last;
  pfq_rwlock_node *rtail[2];
  pfq_rwlock_node *wtail;
  pfq_rwlock_node *whead;
} pfq_rwlock;
