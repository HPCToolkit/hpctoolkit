#include <stdbool.h>

typedef struct pfq_rwlock_node_s {
  struct pfq_rwlock_node_s * volatile next;
  volatile bool blocked;
} mcs_node_t;

typedef mcs_node_t *mcs_lock_t;
