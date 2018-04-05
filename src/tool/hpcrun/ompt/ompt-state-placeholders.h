//***************************************************************************
// global include files
//***************************************************************************

#include <ompt.h>



//***************************************************************************
// local include files
//***************************************************************************

#include "../utilities/ip-normalized.h"



//***************************************************************************
// types
//***************************************************************************

typedef struct {
  void           *pc;
  ip_normalized_t pc_norm; 
} ompt_placeholder_t;


typedef struct {
#define declare_ph(f) ompt_placeholder_t f;
  FOREACH_OMPT_PLACEHOLDER_FN(declare_ph);
#undef declare_ph 
} ompt_placeholders_t; 



//***************************************************************************
// forward declarations
//***************************************************************************

extern ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// interface operations 
//***************************************************************************

void
ompt_init_placeholders(ompt_function_lookup_t ompt_fn_lookup);

