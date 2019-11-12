#ifndef cupti_activity_process_h
#define cupti_activity_process_h



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct cupti_entry_correlation_t cupti_entry_correlation_t;
typedef struct cupti_entry_activity_t cupti_entry_activity_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
cupti_correlation_handle
(
 cupti_entry_correlation_t *entry
);


void
cupti_activity_handle
(
 cupti_entry_activity_t *entry
);

#endif

