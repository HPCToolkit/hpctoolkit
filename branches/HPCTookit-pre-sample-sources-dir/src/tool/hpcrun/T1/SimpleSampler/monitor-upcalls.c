#include "monitor.h"

#include "sample_source.h"
#include "sample_handler.h"
#include "thread_data.h"

/*
 *  Callback functions for the client to override.
 */

void *
monitor_init_process(int *argc, char **argv, void *data)
{
  void *rv = alloc_thread_data(0);
  init_sample_source();
  start_sample_source();
  return rv;
}

void
monitor_fini_process(int how, void *data)
{
  stop_sample_source();
  shutdown_sample_source();
  sample_results();
  free_thread_data();
}

void 
monitor_init_thread_support(void)
{
  ;
}

void *
monitor_init_thread(int tid, void *data)
{
  void *rv = alloc_thread_data(tid);
  start_sample_source();
  return rv;
}

void 
monitor_fini_thread(void *data)
{
  stop_sample_source();
  sample_results();
  free_thread_data();
}
