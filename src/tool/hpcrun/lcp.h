#ifndef LCP_H
#define LCP_H

#include "pmsg.h"

#include "epoch.h"
#include "cct.h"

typedef struct lcp_t {
  csprof_epoch_t*  loadmap;
  hpcrun_cct_t*    cct;
  lush_cct_ctxt_t* ctxt;
} lcp_t;

typedef struct lcp_list_t {
  struct lcp_list_t* next;
  lcp_t*             lcp;
} lcp_list_t;

#endif // LCP_H
