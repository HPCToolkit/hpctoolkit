typedef struct my_concurrent_id_entry_t {
  int a;
  double b;
  char c;
} my_concurrent_id_entry_t;


/* Instantiate concurrent ID map */
#define CONCURRENT_ID_MAP_PREFIX                id_map
#define CONCURRENT_ID_MAP_ENTRY_INITIALIZER     { .a = 5, .b = 11.2, .c = 'a' }
#define CONCURRENT_ID_MAP_ENTRY_TYPE            my_concurrent_id_entry_t
#include "../concurrent-id-map.h"


#include "concurrent-id-map-test.h"
