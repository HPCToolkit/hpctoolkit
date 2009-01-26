#ifndef VALIDATE_RETURN_ADDR_H
#define VALIDATE_RETURN_ADDR_H

#include <stdbool.h>
#include "unwind_cursor.h"

extern bool validate_return_addr(void *addr,unw_cursor_t *cursor);

#endif // VALIDATE_RETURN_ADDR_H
