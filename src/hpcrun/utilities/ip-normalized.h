// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef ip_normalized_h
#define ip_normalized_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "../loadmap.h"

#include "../../common/lean/hpcrun-fmt.h"



//*************************** Forward Declarations **************************

//***************************************************************************

// ---------------------------------------------------------
// ip_normalized_t: A normalized instruction pointer. Since this is a
//   small struct, it can be passed by value.
// ---------------------------------------------------------

typedef struct ip_normalized_t {

  // ---------------------------------------------------------
  // id of load module
  // ---------------------------------------------------------
  uint16_t lm_id;

  // ---------------------------------------------------------
  // static instruction pointer address within the load module
  // ---------------------------------------------------------
  uintptr_t lm_ip;

} ip_normalized_t;


// ---------------------------------------------------------
// comparison operations, mainly for cct sibling splay operations
// ---------------------------------------------------------

static inline bool
ip_normalized_eq(const ip_normalized_t* a, const ip_normalized_t* b)
{
  return ( (a == b) || (a && b
                        && a->lm_id == b->lm_id
                        && a->lm_ip == b->lm_ip) );
}


static inline bool
ip_normalized_lt(const ip_normalized_t* a, const ip_normalized_t* b)
{
  if (a == b) {
    return false;
  }
  const ip_normalized_t ip_norm_0 = {0, 0};
  if (! a) a = &ip_norm_0;
  if (! b) b = &ip_norm_0;

  if (a->lm_id < b->lm_id) return true;
  if (a->lm_id > b->lm_id) return false;
  if (a->lm_ip < b->lm_ip) return true;
  if (a->lm_ip > b->lm_ip) return false;
  return false;
}


static inline bool
ip_normalized_gt(const ip_normalized_t* a, const ip_normalized_t* b)
{
  return ip_normalized_lt(b, a);
}


// Converts an ip into a normalized ip using 'lm'. If 'lm' is NULL
// the function attempts to find the load module that 'unnormalized_ip'
// belongs to. If no load module is found or the load module does not
// have a valid 'dso_info' field, ip_normalized_NULL is returned;
// otherwise, the properly normalized ip is returned.
ip_normalized_t
hpcrun_normalize_ip(void* unnormalized_ip, load_module_t* lm);

void *hpcrun_denormalize_ip(ip_normalized_t *normalized_ip);

//***************************************************************************
// constants
//***************************************************************************

extern const ip_normalized_t ip_normalized_NULL;

#endif // ip_normalized_h
