#include <string.h>

#include "pmsg.h"
#include "sample_source.h"

int
METHOD_FN(csprof_ss_started)
{
  NMSG(SS_COMMON,"check start for %s = %d",self->name,self->state);
  return (self->state == START);
}

void
METHOD_FN(csprof_ss_add_event,const char *ev)
{
  char *evl = self->evl.evl_spec;

  NMSG(SS_COMMON,"add event %s to evl |%s|",ev,evl);
  strcat(evl,ev);
  strcat(evl," ");
  NMSG(SS_COMMON,"evl after event added = |%s|",evl);
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
