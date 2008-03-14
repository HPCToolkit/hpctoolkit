#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csprof_options.h"
#include "csprof_misc_fn_stat.h"
#include "env.h"
#include "pmsg.h"

/* option handling */
/* FIXME: this needs to be split up a little bit for different backends */

int
csprof_options__init(csprof_options_t *x)
{
  memset(x, 0, sizeof(*x));

  x->mem_sz = CSPROF_MEM_SZ_INIT;
  x->event = CSPROF_EVENT;
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

#ifndef CSPROF_PERF
  /* Global option: CSPROF_OPT_VERBOSITY */
  s = getenv(CSPROF_OPT_VERBOSITY);
  if (s) {
    i = atoi(s);
    if ((0 <= i) && (i <= 65536)) {
      CSPROF_MSG_LVL = i;
      fprintf(stderr, "setting message level to %d\n",i);
    }
    else {
      DIE("value of option `%s' [%s] not integer between 0-9", __FILE__, __LINE__,
          CSPROF_OPT_VERBOSITY, s);
    }
  } 

  /* Global option: CSPROF_OPT_DEBUG */
  s = getenv(CSPROF_OPT_DEBUG);
  if (s) {
    i = atoi(s);
    /* FIXME: would like to provide letters as mnemonics, much like Perl */
    CSPROF_DBG_LVL_PUB = i;
  }
#endif

  /* Option: CSPROF_OPT_LUSH_AGENTS */
  s = getenv(CSPROF_OPT_LUSH_AGENTS);
  if (s) {
    strcpy(x->lush_agent_paths, s);
  }
  else {
    x->lush_agent_paths[0] = '\0';
  }

  /* Option: CSPROF_OPT_MAX_METRICS */
  s = getenv(CSPROF_OPT_MAX_METRICS);
  if (s) {
    i = atoi(s);
    if ((0 <= i) && (i <= 10)) {
      x->max_metrics = i;
    }
    else {
      EMSG("value of option `%s' [%s] not integer between 0-10",CSPROF_OPT_MAX_METRICS, s);
      abort();
    }
  }
  else {
    x->max_metrics = 5;
  }

  /* Option: CSPROF_OPT_SAMPLE_PERIOD */
  s = getenv(CSPROF_OPT_SAMPLE_PERIOD);
  if (s) {
    long l;
    char* s1;
    errno = 0; /* set b/c return values on error are all valid numbers! */
    l = strtol(s, &s1, 10);
    // mwf allow 0 as a sample period for debugging
    if (errno != 0 || l < 0 || *s1 != '\0') {
      EMSG("value of option `%s' [%s] is an invalid decimal integer", 
           CSPROF_OPT_SAMPLE_PERIOD, s);
      abort();
    }
    else {
      x->sample_period = l;
    } 
  }
  else {
    x->sample_period = 5000; /* microseconds */
  }

  /* Option: CSPROF_OPT_MEM_SZ */
  s = getenv(CSPROF_OPT_MEM_SZ);
  if(s) {
    unsigned long l;
    char *s1;
    errno = 0;
    l = strtoul(s, &s1, 10);
    if(errno != 0) {
      EMSG("value of option `%s' [%s] is an invalid decimal integer",CSPROF_OPT_MEM_SZ, s);
      abort();
    }
    /* FIXME: may want to consider adding sanity checks (initial memory
       sizes that are too high or too low) */
    if(*s1 == '\0') {
      x->mem_sz = l;
    }
    /* convinience */
    else if(*s1 == 'M' || *s1 == 'm') {
      x->mem_sz = l * 1024 * 1024;
    }
    else if(*s1 == 'K' || *s1 == 'k') {
      x->mem_sz = l * 1024;
    }
    else {
      EMSG("unrecognized memory size unit `%c'",*s1);
      abort();
    }
  }
  else {
    /* provide a reasonable default */
    x->mem_sz = 2 * 1024 * 1024;
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

  /* Option: papi */
  s = getenv(SWITCH_TO_PAPI);
  if (s){
    x->sample_source = PAPI;
    x->papi_event_list = "PAPI_TOT_CYC:10000000";
    s = getenv("PAPI_EVLST");
    if(s){
      x->papi_event_list = s;
    }
  }
  // Option: debug print -- not handled here !!
  return CSPROF_OK;
}
