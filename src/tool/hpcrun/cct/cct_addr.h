#ifndef CCT_ADDR_H
#define CCT_ADDR_H

#include <lib/prof-lean/lush/lush-support.h>
#include <utilities/ip-normalized.h>

typedef struct cct_addr_t cct_addr_t;

struct cct_addr_t {

  lush_assoc_info_t as_info;

  // physical instruction pointer: more accurately, this is an
  // 'operation pointer'.  The operation in the instruction packet is
  // represented by adding 0, 1, or 2 to the instruction pointer for
  // the first, second and third operation, respectively.
  ip_normalized_t ip_norm;

  // logical instruction pointer
  lush_lip_t* lip;
  
};

//
// comparison operations, mainly for cct sibling splay operations
//

static inline bool
cct_addr_eq(const cct_addr_t* a, const cct_addr_t* b)
{
  return ( ip_normalized_eq(&(a->ip_norm), &(b->ip_norm)) &&
           lush_lip_eq(a->lip, b->lip) &&
           lush_assoc_info_eq(a->as_info, b->as_info));
}

static inline bool
cct_addr_lt(const cct_addr_t* a, const cct_addr_t* b)
{
  if (ip_normalized_lt(&(a->ip_norm), &(b->ip_norm))) return true;
  if (ip_normalized_gt(&(a->ip_norm), &(b->ip_norm))) return false;
  if (lush_lip_lt(a->lip, b->lip)) return true;
  if (lush_lip_gt(a->lip, b->lip)) return false;
  if (lush_assoc_info_gt(a->as_info, b->as_info)) return false;
  if (lush_assoc_info_lt(a->as_info, b->as_info)) return true;

  return false;
}

static inline bool
cct_addr_gt(const cct_addr_t* a, const cct_addr_t* b)
{
  return cct_addr_lt(b, a);
}

#define assoc_info_NULL {.bits = 0}

#define NON_LUSH_ADDR_INI(id, ip) {.as_info = assoc_info_NULL, .ip_norm = {.lm_id = id, .lm_ip = ip}, .lip = NULL}

#endif // CCT_ADDR_H
