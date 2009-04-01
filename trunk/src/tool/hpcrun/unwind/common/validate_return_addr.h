#ifndef VALIDATE_RETURN_ADDR_H
#define VALIDATE_RETURN_ADDR_H

#undef _MM
#define _MM(a,v) UNW_ADDR_ ## a = v

typedef enum {
#include "validate_return_addr.src"
} validation_status;

#undef _MM
#define _MM(a,v) [UNW_ADDR_ ## a] = "UNW_ADDR_" #a

static char *_vstat2str_tbl[] = {
#include "validate_return_addr.src"
};

static inline
char *vstat2s(validation_status v) { return _vstat2str_tbl[v]; }

typedef validation_status (*validate_addr_fn_t)(void *addr, void *generic_arg);
extern void hpcrun_validation_summary(void);

#endif // VALIDATE_RETURN_ADDR_H
