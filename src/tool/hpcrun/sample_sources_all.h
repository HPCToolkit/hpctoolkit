#ifndef ALL_SAMPLE_SOURCES_H
#define ALL_SAMPLE_SOURCES_H

#define MAX_SAMPLE_SOURCES 1

void csprof_sample_sources_from_eventlist(char *evl);

void csprof_all_sources_init(void);
void csprof_all_sources_process_event_list(int lush_metrics);
void csprof_all_sources_gen_event_set(int lush_metrics);
void csprof_all_sources_start(void);
void csprof_all_sources_stop(void);
void csprof_all_sources_hard_stop(void);
void csprof_all_sources_shutdown(void);
int  csprof_all_sources_started(void);

#define SAMPLE_SOURCES(op,...) csprof_all_sources_ ##op(__VA_ARGS__)

#endif // ALL_SAMPLE_SOURCES_H
