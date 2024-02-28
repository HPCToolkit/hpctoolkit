#include "../freelist-entry-data.h"


typedef struct my_freelist_entry_t {
  FREELIST_ENTRY_DATA(struct my_freelist_entry_t) freelist;
  int a;
  double b;
  char c;
} my_freelist_entry_t;


/* Instantiate freelist */
#define FREELIST_PREFIX             my_freelist
#define FREELIST_ENTRY_TYPE         my_freelist_entry_t
#define FREELIST_ENTRY_DATA_FIELD   .freelist
#include "../freelist.h"


#include "freelist-test.h"
