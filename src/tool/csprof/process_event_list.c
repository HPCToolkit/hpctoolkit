// Process the event list (a string with substrings of form EVENT:PERIOD separated by spaces)
//   each EVENT:PERIOD substring specifies an event
//
// In particular, the sample source(s) are selected here

// System includes 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local includes
#include "csprof_options.h"
#include "event_info.h"
#include "pmsg.h"
#include "tokenize.h"

void
csprof_process_event_list(const char *evl,event_info *info)
{
  // for now, just look thru the list for wallclock, to set sample period
  // and check to make sure that both wallclock and papi events are not
  // both set
  // FIXME: error checking code needs to be more general
  
  int use_wallclock    = 0;
  int use_papi         = 0;
  unsigned long period = 0L;

  TMSG(EVENTS,"evl (before processing) = |%s|",evl);

  // Option: debug print -- not handled here !!
  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    use_wallclock = (strstr(event,"WALLCLOCK") != NULL);
    if (use_wallclock){
      char *_p = strchr(event,':');
      if (! _p){
        csprof_abort("Syntax for wallclock event is: WALLCLOCK:_Your_Period_Here");
#if 0
        EMSG("Syntax for wallclock event is: WALLCLOCK:_Your_Period_Here");
        abort();
#endif
      }
      period = strtol(_p+1,NULL,10);
      info->sample_period = period;
      TMSG(OPTIONS,"wallclock period set to %ld",period);
    }
    use_papi = use_papi || ! use_wallclock;
    if (use_wallclock && use_papi) {
      csprof_abort("Simultaneous wallclock and papi NOT allowed");
#if 0
      EMSG("Simultaneous wallclock and papi NOT allowed");
      abort();
#endif
    }
  }
  info->sample_source = use_wallclock ? ITIMER : PAPI;
  info->event_list = evl;
  TMSG(EVENTS,"options event list(AFTER processing) = |%s|",info->event_list);
}
