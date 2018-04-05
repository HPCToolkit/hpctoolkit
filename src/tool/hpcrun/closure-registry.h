#ifndef closure_registry_h
#define closure_registry_h
//
//******************************************************************************
// file: closure-registry.h
// purpose: 
//   interface for registering and applying a set of generic closures
//******************************************************************************

//******************************************************************************
// type definitions
//******************************************************************************

typedef void (*closure_fn_t)(void *);


typedef struct closure_s {
  struct closure_s *next;
  closure_fn_t fn;
  void *arg;
} closure_t;


typedef struct closure_list_s {
  closure_t *head;
} closure_list_t;



//******************************************************************************
// interface functions
//******************************************************************************

void closure_list_register(closure_list_t *l, closure_t *entry);

// failure returns -1; success returns 0
int closure_list_deregister(closure_list_t *l, closure_t *entry);

void closure_list_apply(closure_list_t *l);

#endif
