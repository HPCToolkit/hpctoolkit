#ifndef PAPI_SAMPLE_H
#define PAPI_SAMPLE_H

#include "csprof_options.h"

void papi_setup();
void papi_event_init(int *evs,int eventcode,long threshold);
void papi_pulse_init(int evs);
void papi_pulse_fini(void);
void papi_event_info_from_opt(csprof_options_t *opts,int *code,
                              long *thresh);
#endif
