#ifndef PAPI_SAMPLE_H
#define PAPI_SAMPLE_H

#include "csprof_options.h"

void papi_setup();
void papi_event_init(int *evs,char *evlst);
void papi_pulse_init(int evs);
void papi_pulse_fini(void);
#endif
