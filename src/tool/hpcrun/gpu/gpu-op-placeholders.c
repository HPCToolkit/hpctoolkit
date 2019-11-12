//******************************************************************************
// global includes
//******************************************************************************

#include <assert.h>
#include <pthread.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>

#include "gpu-op-placeholders.h"

#if 0
#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>
#endif



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_op_placeholders_t {
  placeholder_t ph[gpu_placeholder_type_count];
} gpu_op_placeholders_t;



//******************************************************************************
// global data
//******************************************************************************

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_none = 0; 

gpu_op_placeholder_flags_t gpu_op_placeholder_flags_all = 
  (~0 << gpu_placeholder_type_count);



//******************************************************************************
// local data
//******************************************************************************

static gpu_op_placeholders_t gpu_op_placeholders;


static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;



//******************************************************************************
// placeholder functions
// 
// note: 
//   placeholder functions are not declared static so that the compiler 
//   doesn't eliminate their names from the symbol table. we need their
//   names in the symbol table to convert them into the appropriate placeholder 
//   strings in hpcprof
//******************************************************************************

void 
gpu_op_copy
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_copyin
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_copyout
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void 
gpu_op_alloc
(
  void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_delete
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_kernel
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_sync
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


void
gpu_op_trace
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}



//******************************************************************************
// private operations
//******************************************************************************

static void 
gpu_op_placeholder_init
(
 gpu_placeholder_type_t type,
 void *pc
 )
{
  init_placeholder(&gpu_op_placeholders.ph[type], pc);
}


static void
gpu_op_placeholders_init
(
 void
)
{
  gpu_op_placeholder_init(gpu_placeholder_type_copy,    &gpu_op_copy);
  gpu_op_placeholder_init(gpu_placeholder_type_copyin,  &gpu_op_copyin);
  gpu_op_placeholder_init(gpu_placeholder_type_copyout, &gpu_op_copyout);
  gpu_op_placeholder_init(gpu_placeholder_type_alloc,   &gpu_op_alloc);
  gpu_op_placeholder_init(gpu_placeholder_type_delete,  &gpu_op_delete);
  gpu_op_placeholder_init(gpu_placeholder_type_kernel,  &gpu_op_kernel);
  gpu_op_placeholder_init(gpu_placeholder_type_sync,    &gpu_op_sync);
  gpu_op_placeholder_init(gpu_placeholder_type_trace,   &gpu_op_trace);
}



//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
gpu_op_placeholder_ip
(
 gpu_placeholder_type_t type
)
{
  pthread_once(&is_initialized, gpu_op_placeholders_init);

  return gpu_op_placeholders.ph[type].pc_norm;
}


void
gpu_op_ccts_insert
(
 cct_node_t *api_node,
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_op_placeholder_flags_t flags
)
{
  int i;
  cct_addr_t frm;

  memset(&frm, 0, sizeof(cct_addr_t));

  for (i = 0; i < gpu_placeholder_type_count; i++) {
    cct_node_t *node = NULL;
    if (flags & (1 << i)) {
      frm.ip_norm = gpu_op_placeholder_ip(i);
      node = hpcrun_cct_insert_addr(api_node, &frm);
    }
    gpu_op_ccts->ccts[i] = node;
  }
}


void
gpu_op_placeholder_flags_set
(
 gpu_op_placeholder_flags_t *flags,
 gpu_placeholder_type_t type
)
{
  *flags |= (1 << type);
}


bool
gpu_op_placeholder_flags_is_set
(
 gpu_op_placeholder_flags_t flags,
 gpu_placeholder_type_t type
)
{
  return (flags & (1 << type)) ? true : false;
}
