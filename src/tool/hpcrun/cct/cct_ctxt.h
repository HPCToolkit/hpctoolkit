#ifndef CCT_CTXT_H
#define CCT_CTXT_H
#include <stdint.h>

#include <cct/cct.h>

//
// data type [abstract part]
//

typedef struct cct_ctxt_t cct_ctxt_t;

// Represents the creation context of a given calling context tree as
// a linked list of cct's

//
// data type [concrete part]
// NB: NOT opaque
//
struct cct_ctxt_t {
  cct_node_t* context;// cct  
  cct_ctxt_t* parent; // a list of cct_ctxt_t

};

//
// Interface routines
//
extern void walk_ctxt_rev(cct_ctxt_t* ctxt, cct_op_t op, cct_op_arg_t arg);
extern cct_ctxt_t* copy_thr_ctxt(cct_ctxt_t* thr_ctxt);

#endif // CCT_CTXT_H
