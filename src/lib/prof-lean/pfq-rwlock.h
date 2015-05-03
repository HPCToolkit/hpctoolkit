#include <inttypes.h>
#include <stdbool.h>

#include "mcs-lock.h"

typedef mcs_node_t pfq_rwlock_node_t;

// align a variable at the start of a cache line
// CLA = Cache Line Aligned
#define CLA(x) x __attribute__((aligned(128)))

typedef struct {
  CLA(volatile bool flag);
} pfq_rwlock_flag_t;

typedef struct {
  //----------------------------------------------------------------------------
  // reader management
  //----------------------------------------------------------------------------
  CLA(uint32_t rin);
  CLA(uint32_t rout);
  CLA(uint32_t last);

  pfq_rwlock_flag_t flag[2]; 

  //----------------------------------------------------------------------------
  // writer management
  //----------------------------------------------------------------------------
  CLA(mcs_lock_t wtail);
  CLA(mcs_node_t *whead);

} pfq_rwlock_t;
