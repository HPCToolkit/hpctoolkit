#ifndef CCT_BUNDLE_H
#define CCT_BUNDLE_H
#include <stdbool.h>

#include "cct.h"
#include "cct_ctxt.h"

//
// Data type not opaque : FIXME ??
//

typedef struct cct_bundle_t {

  cct_node_t* top;                // top level tree (at creation)
  cct_node_t* tree_root;          // main cct for full unwinds from samples
                                  // in unthreaded case tree_root = top,
                                  // in threaded case, it is the tree obtained from inserting
                                  // the creation context
  cct_node_t* partial_unw_root;   // adjunct tree for partial unwinds
  cct_ctxt_t* ctxt;               // creation context for bundle
  unsigned long num_nodes;        // utility to count nodes. NB: MIGHT go away
} cct_bundle_t;

//
// Interface procedures
//

extern void hpcrun_cct_bundle_init(cct_bundle_t* bundle, cct_ctxt_t* ctxt);
//
// IO for cct bundle
//
extern int hpcrun_cct_bundle_fwrite(FILE* fs, epoch_flags_t flags, cct_bundle_t* x);

//
// utility functions
//
extern bool hpcrun_empty_cct(cct_bundle_t* cct);

#endif // CCT_BUNDLE_H
