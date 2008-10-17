#ifndef REGISTERED_SAMPLE_SOURCES_H
#define REGISTERED_SAMPLE_SOURCES_H

#include "sample_source.h"
#define MAX_POSSIBLE_SAMPLE_SOURCES 2

extern void             csprof_ss_register(sample_source_t *src);
extern sample_source_t *csprof_source_can_process(char *event);
// extern void             csprof_init_sample_source_registration(void);
extern void csprof_registered_sources_init(void);
extern void csprof_registered_sources_list(void);

#endif // REGISTERED_SAMPLE_SOURCES_H
