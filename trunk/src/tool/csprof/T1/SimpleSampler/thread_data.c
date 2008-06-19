#include <stdlib.h>

#include "thread_data.h"
#include "monitor.h"

static thread_data_t local_data = {
  .id    = 0,
  .count = 0
};

thread_data_t *
get_thread_data(void)
{
  return (thread_data_t *) monitor_get_user_data();
}

void *
alloc_thread_data(unsigned int id)
{
  thread_data_t *t = &local_data;

  if (id){
    t = (thread_data_t *) malloc(sizeof(thread_data_t));
  }

  t->id = id;
  t->count = 0;

  return (void *)t;
}

void
free_thread_data(void)
{
  thread_data_t *t = get_thread_data();
  if (t->id) {
    free((void *) t);
  }
}
