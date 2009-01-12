#ifndef STACK_TROLL_H
#define STACK_TROLL_H

#include <include/general.h>

#ifdef __cplusplus
extern "C" {
#endif

uint stack_troll(void **start_sp, uint *ra_pos);

#ifdef __cplusplus
}
#endif

#define TROLL_LIMIT 16

#endif /* STACK_TROLL_H */
