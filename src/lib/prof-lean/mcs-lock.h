#include <stdbool.h>

typedef struct mcs_node_s {
  struct mcs_node_s * volatile next;
  volatile bool blocked;
} mcs_node_t;

typedef mcs_node_t *mcs_lock_t;
