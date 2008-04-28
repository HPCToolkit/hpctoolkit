#include <string.h>

#include "pmsg.h"
#include "sample_source.h"

void
METHOD_FN(csprof_ss_add_event,const char *ev)
{
  strcpy(self->evl.evl_spec,ev);
  strcat(self->evl.evl_spec," ");
}

void
METHOD_FN(csprof_ss_store_event,int event_id,long thresh)
{
  evlist_t *_p       = &(self->evl);
  int *ev  = &(_p->nevents);
  if (*ev >= MAX_EVENTS) {
    EMSG("Too many events entered for sample source. Event code %d ignored",event_id);
    return;
  }
  _ev_t *current_event  = &(_p->events[*ev]);

  current_event->event  = event_id;
  current_event->thresh = thresh;

  (*ev)++;
}

char *
METHOD_FN(csprof_ss_get_event_str)
{
  return (self->evl).evl_spec;
}
