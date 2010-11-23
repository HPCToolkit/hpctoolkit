#include <stdint.h>

#include <messages/messages.h>
#include "cct_ctxt.h"

static void
walk_pth_rev(cct_node_t* path, cct_op_t op, cct_op_arg_t arg, size_t level)
{
  if (! path) return;

  walk_pth_rev(hpcrun_cct_parent(path), op, arg, level);
  op(path, arg, level);
}

static void
walk_ctxt_rev_l(cct_ctxt_t* ctxt, cct_op_t op, cct_op_arg_t arg, size_t level)
{
  if (! ctxt) return;

  walk_ctxt_rev_l(ctxt->parent, op, arg, level+1);
  walk_pth_rev(ctxt->context, op, arg, level);
}

void
walk_ctxt_rev(cct_ctxt_t* ctxt, cct_op_t op, cct_op_arg_t arg)
{
  walk_ctxt_rev_l(ctxt, op, arg, 0);
}

cct_ctxt_t* 
copy_thr_ctxt(cct_ctxt_t* thr_ctxt)
{
  // MEMORY PROBLEM: if thr_ctxt is reclaimed 
  // FIXME: if reclamation of a thread context is possible, we would need
  //        a deep copy here.
  return thr_ctxt;
}
