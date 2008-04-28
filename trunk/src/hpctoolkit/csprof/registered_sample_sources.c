#include <stdlib.h>

#include "pmsg.h"
#include "pre_registered_sources.h"
#include "registered_sample_sources.h"
#include "sample_source.h"

static sample_source_t *registered_sample_sources[MAX_POSSIBLE_SAMPLE_SOURCES];

static int nregs = 0;

void
csprof_ss_register(sample_source_t *src)
{
  if (nregs >= MAX_POSSIBLE_SAMPLE_SOURCES){
    EMSG("Sample source named %s NOT registered due to # sample sources exceeded",src->name);
    return;
  }
  registered_sample_sources[nregs++] = src;
}


sample_source_t *
csprof_source_can_process(char *event)
{
  for (int i=0;i < nregs;i++){
    if (METHOD_CALL(registered_sample_sources[i],supports_event,event)){
      return registered_sample_sources[i];
    }
  }
  return NULL;
}

// FIXME: constructor attribute doesn't work for LD_PRELOAD ??
void
csprof_init_sample_source_registration(void)
{
  for (int i=0; i < N_PRE_REGISTERED; i++){
    csprof_ss_register(pre_registered_sources[i]);
  }
}
