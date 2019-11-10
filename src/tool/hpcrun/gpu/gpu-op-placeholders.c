//******************************************************************************
// global includes
//******************************************************************************

#include <assert.h>
#include <pthread.h>



//******************************************************************************
// local includes
//******************************************************************************

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
gpu_op_sync
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
