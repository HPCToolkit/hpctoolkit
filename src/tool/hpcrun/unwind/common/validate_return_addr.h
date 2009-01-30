#ifndef VALIDATE_RETURN_ADDR_H
#define VALIDATE_RETURN_ADDR_H

#include <stdbool.h>
#include "unwind_cursor.h"
typedef enum {
  UNW_ADDR_CONFIRMED = 0,
  UNW_ADDR_PROBABLE  = 1,
  UNW_ADDR_CYCLE     = 2,
  UNW_ADDR_WRONG     = 3
} validation_status;

typedef validation_status (*validate_addr_fn_t)(void *addr, void *generic_arg);

#endif // VALIDATE_RETURN_ADDR_H
