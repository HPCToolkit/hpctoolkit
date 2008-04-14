#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csprof_options.h"
#include "csprof_misc_fn_stat.h"
#include "env.h"
#include "pmsg.h"
#include "tokenize.h"

/* option handling */
/* FIXME: this needs to be split up a little bit for different backends */

int
csprof_options__init(csprof_options_t *x)
{
  memset(x, 0, sizeof(*x));

  // x->mem_sz = CSPROF_MEM_SZ_INIT;
  // x->event = CSPROF_EVENT;
  x->sample_period = CSPROF_SMPL_PERIOD;
  x->sample_source = ITIMER;
  // x->sample_source = PAPI;
  
  return CSPROF_OK;
}

int
csprof_options__fini(csprof_options_t* x)
{
  return CSPROF_OK;
}

/* assumes no private 'heap' memory is available yet */
int
csprof_options__getopts(csprof_options_t* x)
{

  char tmp[CSPROF_PATH_SZ];
  char* s;
  int i = 0;

  /* Option: CSPROF_OPT_LUSH_AGENTS */
  s = getenv(CSPROF_OPT_LUSH_AGENTS);
  if (s) {
    strcpy(x->lush_agent_paths, s);
  }
  else {
    x->lush_agent_paths[0] = '\0';
  }

  /* Option: CSPROF_OPT_OUT_PATH */
  s = getenv(CSPROF_OPT_OUT_PATH);
  if (s) {
    i = strlen(s);
    if(i==0) {
      strcpy(tmp, ".");
    }
    if((i + 1) > CSPROF_PATH_SZ) {
      EMSG("value of option `%s' [%s] has a length greater than %d",CSPROF_OPT_OUT_PATH, s, CSPROF_PATH_SZ);
      abort();
    }
    strcpy(tmp, s);
  }
  else {
    strcpy(tmp, ".");
  }
  
  if (realpath(tmp, x->out_path) == NULL) {
    EMSG("could not access path `%s': %s", tmp, strerror(errno));
    abort();
  }

  // process event list
  // for now, just look thru the list for wallclock, to set sample period
  // and check to make sure that both wallclock and papi events are not
  // both set
  // FIXME: error checking code needs to be more general
  
  int use_wallclock    = 0;
  int use_papi         = 0;
  unsigned long period = 0L;

  char *evl         = getenv("CSPROF_OPT_EVENT");
  TMSG(OPTIONS,"evl (before processing) = |%s|",evl);
  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    use_wallclock = (strstr(event,"WALLCLOCK") != NULL);
    if (use_wallclock){
      char *_p = strchr(event,':');
      if (! _p){
        EMSG("Syntax for wallclock event is: WALLCLOCK:_Your_Period_Here");
        abort();
      }
      period = strtol(_p+1,NULL,10);
      TMSG(OPTIONS,"wallclock period set to %ld",period);
    }
    use_papi = use_papi || ! use_wallclock;
    if (use_wallclock && use_papi) {
      EMSG("Simultaneous wallclock and papi NOT allowed");
      abort();
    }
  }
  x->sample_source = use_wallclock ? ITIMER : PAPI;
  x->event_list = evl;
  TMSG(OPTIONS,"options event list(AFTER processing) = |%s|",x->event_list);

  // Option: debug print -- not handled here !!
  return CSPROF_OK;
}
