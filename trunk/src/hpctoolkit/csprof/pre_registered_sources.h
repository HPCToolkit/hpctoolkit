#ifndef PRE_REGISTERED_SOURCES_H
#define PRE_REGISTERED_SOURCES_H

#include "itimer.sample_source.h"
#include "papi.sample_source.h"
#include "sample_source.h"

static sample_source_t *pre_registered_sources[] = {
  &_itimer_obj,
  &_papi_obj,
};

#define N_PRE_REGISTERED (sizeof(pre_registered_sources)/sizeof(sample_source_t *))

#endif // PRE_REGISTERED_SOURCES_H
