#ifndef cuda_state_placeholders_h
#define cuda_state_placeholders_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct {
  placeholder_t cuda_none_state;
  placeholder_t cuda_copy_state;
  placeholder_t cuda_alloc_state;
  placeholder_t cuda_delete_state;
  placeholder_t cuda_kernel_state;
  placeholder_t cuda_sync_state;
} cuda_placeholders_t;



//******************************************************************************
// forward declarations for global data 
//******************************************************************************

extern cuda_placeholders_t cuda_placeholders;



//******************************************************************************
// interface operations
//******************************************************************************

void
cuda_init_placeholders
(
);



#endif
