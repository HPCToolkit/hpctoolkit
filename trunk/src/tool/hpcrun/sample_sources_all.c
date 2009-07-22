//
// TODO: NONE unimplemented ????
//


//
// The sample sources data structure
//

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "simple_oo.h"
#include "sample_source.h"
#include "sample_sources_all.h"
#include "sample_sources_registered.h"
#include "tokenize.h"

#include <messages/messages.h>

#define csprof_event_abort(...) \
  csprof_abort_w_info(csprof_registered_sources_list, __VA_ARGS__)

//
// FUNCTION DEFINING MACROS
//

#define _AS0(n) \
void                                                            \
csprof_all_sources_ ##n(void)                                   \
{								\
  for(int i=0;i < n_sources;i++) {				\
    TMSG(AS_MAP,"sample source %d (%s) method call: %s",i,	\
	 sample_sources[i]->name,#n);				\
    METHOD_CALL(sample_sources[i],n);				\
  }								\
}

#define _AS1(n,t,arg) \
void                                                            \
csprof_all_sources_ ##n(t arg)                                  \
{								\
  for(int i=0;i < n_sources;i++) {				\
    METHOD_CALL(sample_sources[i],n,arg);			\
  }								\
}

#define _ASB(n)							\
bool								\
csprof_all_sources_ ##n(void)					\
{								\
  NMSG(AS_ ##n,"checking %d sources",n_sources);		\
  for(int i=0;i < n_sources;i++) {				\
    if (! METHOD_CALL(sample_sources[i],n)) {			\
      NMSG(AS_ ##n,"%s not started",sample_sources[i]->name);	\
      return false;						\
    }								\
  }								\
  return true;							\
}

//
// END Function Defining Macros
//

static sample_source_t *sample_sources[MAX_SAMPLE_SOURCES];

static int n_sources = 0;

bool
csprof_check_named_source(const char *src)
{
  for(int i=0; i < n_sources; i++){
    if (strcmp(sample_sources[i]->name, src) == 0) {
      return true;
    }
  }
  return false;
}

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
      csprof_abort("Too many sample sources");
    }
    return;
  }
  sample_sources[n_sources] = ss;
  n_sources++;
  NMSG(AS_add_source,"# sources now = %d",n_sources);
}


void
csprof_sample_sources_from_eventlist(char *evl)
{
  if (evl == 0){
    csprof_abort("*** No sampling sources are specified --- aborting");
    return;
  }

  TMSG(EVENTS,"evl (before processing) = |%s|",evl);

  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    sample_source_t *s;
    if ( (s = csprof_source_can_process(event)) ){
      add_source(s);
      METHOD_CALL(s,add_event,event);
    }
    else {
      csprof_event_abort("Requested event %s is not supported!",event);
    }
  }
}


// The mapped operations

_AS1(process_event_list,int,lush_metrics)
_AS0(init)
_AS1(gen_event_set,int,lush_metrics)
_AS0(start)
_AS0(stop)
_AS0(hard_stop)
_AS0(shutdown)
_ASB(started)
