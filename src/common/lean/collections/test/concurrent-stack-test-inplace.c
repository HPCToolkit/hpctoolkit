#include <stdlib.h>
#include "../concurrent-stack-entry-data.h"

#define VALUE_TYPE int


typedef struct my_cstack_entry_t {
  CONCURRENT_STACK_ENTRY_DATA(struct my_cstack_entry_t);
  size_t idx;
  size_t thread_id;
  VALUE_TYPE value;
} my_cstack_entry_t;


/* Instantiate concurrent_stack */
#define CONCURRENT_STACK_PREFIX         my_cstack
#define CONCURRENT_STACK_ENTRY_TYPE     my_cstack_entry_t
#define CONCURRENT_STACK_DEFINE_INPLACE
#include "../concurrent-stack.h"


#include "concurrent-stack-test.h"
