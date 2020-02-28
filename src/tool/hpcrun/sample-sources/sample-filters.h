#ifndef sample_filters_h
#define sample_filters_h

typedef int (*sf_fn_t)(void* arg);

typedef struct sf_fn_entry_t {
  struct sf_fn_entry_t* next;
  sf_fn_t fn;
  void* arg;
} sf_fn_entry_t;


void sample_filters_register(sf_fn_entry_t *entry);

int sample_filters_apply();

#endif
