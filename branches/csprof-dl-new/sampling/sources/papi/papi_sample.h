#ifndef PAPI_SAMPLE_H
#define PAPI_SAMPLE_H

#include "csprof_options.h"

void papi_setup(void);
void papi_parse_evlist(char *evlst);
int papi_event_init(void);
void papi_pulse_init(int evs);
void papi_pulse_fini(int evs);
void papi_pulse_shutdown(int evs);
#endif
