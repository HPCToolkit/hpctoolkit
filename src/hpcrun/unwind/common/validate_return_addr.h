// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef VALIDATE_RETURN_ADDR_H
#define VALIDATE_RETURN_ADDR_H

#define VSTAT_ENUMS \
  _MM(CONFIRMED) \
  _MM(PROBABLE_INDIRECT) \
  _MM(PROBABLE_TAIL) \
  _MM(PROBABLE) \
  _MM(CYCLE) \
  _MM(WRONG)


typedef enum {
#define _MM(a) UNW_ADDR_ ## a,
VSTAT_ENUMS
#undef _MM
} validation_status;


static char *_vstat2str_tbl[] = {
#define _MM(a) [UNW_ADDR_ ## a] = "UNW_ADDR_" #a,
VSTAT_ENUMS
#undef _MM
};

static inline
char *vstat2s(validation_status v) { return _vstat2str_tbl[v]; }

typedef validation_status (*validate_addr_fn_t)(void *addr, void *generic_arg);
extern void hpcrun_validation_summary(void);

#endif // VALIDATE_RETURN_ADDR_H
