#ifndef gpu_op_placeholders_h
#define gpu_op_placeholders_h



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum gpu_placeholder_type_t {
  gpu_placeholder_type_copy    = 0, // general copy, d2d d2a, or a2d
  gpu_placeholder_type_copyin  = 1,
  gpu_placeholder_type_copyout = 2,
  gpu_placeholder_type_alloc   = 3,
  gpu_placeholder_type_delete  = 4,
  gpu_placeholder_type_kernel  = 5,
  gpu_placeholder_type_sync    = 6,

  gpu_placeholder_type_count   = 7
} gpu_placeholder_type_t;



//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
gpu_op_placeholder_ip
(
 gpu_placeholder_type_t type
);



#endif
