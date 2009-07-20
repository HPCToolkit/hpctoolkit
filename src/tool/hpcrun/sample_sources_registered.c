#include <stdio.h>
#include <stdlib.h>

#include "sample_sources_registered.h"
#include "sample_source.h"

#include <messages/messages.h>

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

void
csprof_registered_sources_init(void)
{
  for (int i=0;i<nregs;i++){
    METHOD_CALL(registered_sample_sources[i],init);
    NMSG(SS_COMMON,"sample source \"%s\": init",registered_sample_sources[i]->name);
  }
}

void
csprof_registered_sources_list(void)
{
  fprintf(stderr,"Registered Sample Sources:\n");
  for (int i=0;i<nregs;i++){
    fprintf(stderr,"    %s\n",registered_sample_sources[i]->name);
  }
}
