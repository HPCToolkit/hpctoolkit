#ifndef placeholders_h
#define placeholders_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/utilities/ip-normalized.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct {
    void           *pc;
    ip_normalized_t pc_norm;
} placeholder_t;


typedef enum hpcrun_placeholder_type_t {
  hpcrun_placeholder_type_no_activity    = 0, 
  hpcrun_placeholder_type_count          = 1 
} hpcrun_placeholder_type_t;




//******************************************************************************
// interface operations
//******************************************************************************

load_module_t *
pc_to_lm
(
 void *pc
);


void
init_placeholder
(
 placeholder_t *p,
 void *pc
);


placeholder_t *
hpcrun_placeholder_get
(
 hpcrun_placeholder_type_t ph_type
);



#endif
