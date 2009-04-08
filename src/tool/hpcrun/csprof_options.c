//
// $Id$
//

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csprof_options.h"
#include "files.h"
#include "process_event_list.h"
#include "csprof_misc_fn_stat.h"
#include "env.h"
#include "pmsg.h"
#include "sample_sources_all.h"
#include "sample_sources_registered.h"

/* option handling */
/* FIXME: this needs to be split up a little bit for different backends */

int
csprof_options__init(csprof_options_t *x)
{
  NMSG(OPTIONS,"__init");
  memset(x, 0, sizeof(*x));
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
  /* Option: CSPROF_OPT_LUSH_AGENTS */
  char *s = getenv(CSPROF_OPT_LUSH_AGENTS);
  if (s) {
    strcpy(x->lush_agent_paths, s);
  }
  else {
    x->lush_agent_paths[0] = '\0';
  }

  // process event list
  // HPCRUN_EVENT_LIST is the approved name, but accept CSPROF_OPT_EVENT
  // for backwards compatibility for now.
  
  NMSG(OPTIONS,"--before init of registered sample sources");
  csprof_registered_sources_init();
  if ((s = getenv(HPCRUN_EVENT_LIST)) == NULL)
    s = getenv("CSPROF_OPT_EVENT");
  csprof_sample_sources_from_eventlist(s);

  return CSPROF_OK;
}
