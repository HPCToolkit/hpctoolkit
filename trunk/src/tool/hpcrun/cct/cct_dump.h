#ifndef CCT_DUMP_H
#define CCT_DUMP_H
#ifndef _FLG
#define _FLG CCT
#endif

#include <stdint.h>

#include <cct/cct.h>
#include <messages/messages.h>

#define TLIM 1

#ifndef TLIM
#define TL(E)  E
#define TLS(S) S
#else
#include <hpcrun/thread_data.h>
#define TL(E) ((TD_GET(id) == TLIM) && (E))
#define TLS(S) if (TD_GET(id) == TLIM) S
#endif


#define TF(_FLG, ...) TMSG(_FLG, __VA_ARGS__)

static void
l_dump(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  char p[level+1];

  if (! node) return;

  memset(&p[0], ' ', level);
  p[level] = '\0';

  int32_t id = hpcrun_cct_persistent_id(node);
  cct_addr_t* adr = hpcrun_cct_addr(node);

  TF(_FLG, "%s[%d: (%d, %p)]", &p[0], id,
     adr->ip_norm.lm_id, adr->ip_norm.lm_ip);
}

static void
dump_cct(cct_node_t* cct)
{
  hpcrun_cct_walk_node_1st(cct, l_dump, NULL);
}

#endif // CCT_DUMP_H
