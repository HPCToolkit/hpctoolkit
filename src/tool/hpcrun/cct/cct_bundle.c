#include <stdbool.h>

#include "cct_bundle.h"
#include <lib/prof-lean/hpcrun-fmt.h>
#include <cct/cct_addr.h>
#include <messages/messages.h>
#include <hpcrun/hpcrun_return_codes.h>

//
// convenient constant cct_addr_t's
//

static cct_addr_t root = ADDR_I(CCT_ROOT);

//
// Private Operations
//

//
// helper for inserting creation contexts
//
static void
l_insert_path(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  cct_addr_t* addr = hpcrun_cct_addr(node);
  if (cct_addr_eq(addr, &root)) return;

  cct_node_t** tree = (cct_node_t**) arg;
  *tree = hpcrun_cct_insert_addr(*tree, addr);
}
//
// Interface procedures
//
void
hpcrun_cct_bundle_init(cct_bundle_t* bundle, cct_ctxt_t* ctxt)
{
  bundle->top = hpcrun_cct_new();
  bundle->tree_root = bundle->top;
  bundle->ctxt = ctxt;
  bundle->num_nodes = 0;
  //
  // If there is a creation context (ie, this is a pthread),
  // insert the creation context in the cct, and attach all call paths
  // to the call context prefix node instead of the top of the tree.
  //
  if (ctxt) {
    hpcrun_walk_path(ctxt->context, l_insert_path, (cct_op_arg_t) &(bundle->tree_root));
  }
  bundle->partial_unw_root = hpcrun_cct_new_partial();
}
//
// Write to file for cct bundle: 
//
int 
hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* bndl)
{
  if (!fs) { return HPCRUN_ERR; }

  cct_node_t* final = bndl->tree_root;
  cct_node_t* partial_insert = final;

  //
  // attach partial unwinds at appointed slot
  //
  hpcrun_cct_insert_node(partial_insert, bndl->partial_unw_root);

  // write out newly constructed cct

  return hpcrun_cct_fwrite(bndl->top, fs, flags);
}

//
// cct_fwrite helpers
//

//
// utility functions
//
bool
hpcrun_empty_cct(cct_bundle_t* cct)
{
  bool rv = (cct->num_nodes <= 1);
  if (rv) {
    TMSG(CCT, "cct %p is empty", cct);
  }
  return rv;
}
