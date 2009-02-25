#ifndef STACK_TROLL_H
#define STACK_TROLL_H

#include <include/uint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TROLL_VALID  = 0, // trolled-for address provably valid
  TROLL_LIKELY,     // trolled-for address could not be proved invalid
  TROLL_INVALID     // all trolled-for addresses were invalid
} troll_status;

#include "validate_return_addr.h"
  troll_status stack_troll(void **start_sp, uint *ra_pos, validate_addr_fn_t validate_addr, void *generic_arg);

#ifdef __cplusplus
}
#endif

#define TROLL_LIMIT 16

#endif /* STACK_TROLL_H */
