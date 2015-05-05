#ifndef _mcs_lock_h_
#define _mcs_lock_h_
//******************************************************************************
// MCS lock - a fair based queueing lock
//
// Reference:
//  John M. Mellor-Crummey and Michael L. Scott. 1991. Algorithms for scalable 
//  synchronization on shared-memory multiprocessors. ACM Transactions on 
//  Computing Systems 9, 1 (February 1991), 21-65. 
//  http://doi.acm.org/10.1145/103727.103729
//******************************************************************************

//******************************************************************************
// global includes 
//******************************************************************************

#include <stdbool.h>



//******************************************************************************
// types 
//******************************************************************************

typedef struct mcs_node_s {
  struct mcs_node_s * volatile next;
  volatile bool blocked;
} mcs_node_t;


typedef struct {
  volatile mcs_node_t *tail;
} mcs_lock_t;



//******************************************************************************
// interface functions
//******************************************************************************

void
mcs_lock(mcs_lock_t *l, mcs_node_t *me);

void
mcs_unlock(mcs_lock_t *l, mcs_node_t *me);



#endif
