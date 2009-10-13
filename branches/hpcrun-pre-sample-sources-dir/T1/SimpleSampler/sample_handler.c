#include <stdio.h>
#include <stdlib.h>

#include "sample_handler.h"
#include "thread_data.h"

void
sample_results(void)
{
  thread_data_t *t = get_thread_data();
  fprintf(stderr,"[%d]: sample count = %d\n",t->id,t->count);
}

void
process_sample(void *in)
{
  thread_data_t *t = get_thread_data();
  (t->count)++;
}
