#ifndef ALL_SAMPLE_SOURCES_H
#define ALL_SAMPLE_SOURCES_H

#define MAX_SAMPLE_SOURCES 1

#include <stdbool.h>

#include "sample_source.h"

extern void csprof_sample_sources_from_eventlist(char *evl);
extern sample_source_t* hpcrun_fetch_source_by_name(const char *src);
extern bool csprof_check_named_source(const char *src);
extern void csprof_all_sources_init(void);
extern void csprof_all_sources_process_event_list(int lush_metrics);
extern void csprof_all_sources_gen_event_set(int lush_metrics);
extern void csprof_all_sources_start(void);
extern void csprof_all_sources_stop(void);
extern void csprof_all_sources_hard_stop(void);
extern void csprof_all_sources_shutdown(void);
extern bool  csprof_all_sources_started(void);

#define SAMPLE_SOURCES(op,...) csprof_all_sources_ ##op(__VA_ARGS__)

#endif // ALL_SAMPLE_SOURCES_H
