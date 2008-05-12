//
// The sample sources data structure
//
// Even though (as of 24apr08) the current system is limited to 1 source,
// we use an array for extensibility

#include <stdlib.h>

#include "all_sample_sources.h"
#include "pmsg.h"
#include "registered_sample_sources.h"
#include "simple_oo.h"
#include "sample_source.h"
#include "tokenize.h"

static sample_source_t *sample_sources[MAX_SAMPLE_SOURCES];

static int n_sources = 0;

static int
in_sources(sample_source_t *ss)
{
  for(int i=0; i < n_sources; i++){
    if (sample_sources[i] == ss)
      return 1;
  }
  return 0;
}

static void
add_source(sample_source_t *ss)
{
  NMSG(AS_add_source,"%s",ss->name);
  if (n_sources == MAX_SAMPLE_SOURCES){
    // check to see is ss already present
    if (! in_sources(ss)){
      EMSG("Too many sample sources");
      abort();
    }
    return;
  }
  sample_sources[n_sources] = ss;
  n_sources++;
  NMSG(AS_add_source,"# sources now = %d",n_sources);
}

#define _AS0(n) \
void                                                          \
csprof_all_sources_ ##n(void)                                 \
{                                                             \
  for(int i=0;i < n_sources;i++) {                            \
    METHOD_CALL(sample_sources[i],n);                  \
  }                                                           \
}

#define _AS1(n,t,arg) \
void                                                          \
csprof_all_sources_ ##n(t arg)                                \
{                                                             \
  for(int i=0;i < n_sources;i++) {                            \
    METHOD_CALL(sample_sources[i],n,arg);              \
  }                                                           \
}

#define _ASB(n) \
int                                                \
csprof_all_sources_ ##n(void)                      \
{                                                  \
  NMSG(AS_ ##n,"checking %d sources",n_sources);   \
  for(int i=0;i < n_sources;i++) {                 \
    if (! METHOD_CALL(sample_sources[i],n)) {      \
      NMSG(AS_ ##n,"%s not started",sample_sources[i]->name); \
      return 0;                                    \
    }                                              \
  }                                                \
  return 1;                                        \
}

// The mapped operations

_AS0(process_event_list)
_AS0(init)
_AS1(gen_event_set,int,lush_metrics)
_AS0(start)
_AS0(stop)
_AS0(shutdown)
_ASB(started)

void
csprof_sample_sources_from_eventlist(char *evl)
{
  TMSG(EVENTS,"evl (before processing) = |%s|",evl);

  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    sample_source_t *s;
    if ( (s = csprof_source_can_process(event)) ){
      add_source(s);
      METHOD_CALL(s,add_event,event);
    }
    else {
      EMSG("Requested event %s is not supported!",event);
      abort();
    }
  }
}
