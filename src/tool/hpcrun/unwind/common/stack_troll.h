#ifndef STACK_TROLL_H
#define STACK_TROLL_H

#include <include/general.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "unwind_cursor.h"
  int stack_troll(void **start_sp, uint *ra_pos,unw_cursor_t *cursor);

#ifdef __cplusplus
}
#endif

#define TROLL_LIMIT 16

#endif /* STACK_TROLL_H */
