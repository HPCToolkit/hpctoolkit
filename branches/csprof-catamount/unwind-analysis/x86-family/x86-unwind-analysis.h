#ifndef x86_unwind_analysis_h
#define x86_unwind_analysis_h

#include "xed-interface.h"

#include "intervals.h"
#include "find.h"

#include "csprof-malloc.h"


/******************************************************************************
 * local types
 *****************************************************************************/

typedef enum {HW_NONE, HW_BRANCH, HW_CALL, HW_BPSAVE, HW_SPSUB, HW_CREATE_STD} 
  hw_type;

typedef struct highwatermark_s {
  unwind_interval *uwi;
  hw_type type;
} highwatermark_t;

#define PREFER_BP_FRAME 0


/******************************************************************************
 * macros 
 *****************************************************************************/
#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)
#define iclass_eq(xptr, class) (iclass(xptr) == (class))


#endif



