#ifndef COMMON_SAMPLE_SOURCE_H
#define COMMON_SAMPLE_SOURCE_H

#include "sample_source.h"

extern void  METHOD_FN(csprof_ss_add_event,const char *ev);
extern void  METHOD_FN(csprof_ss_store_event,int event_id,long thresh);
extern char *METHOD_FN(csprof_ss_get_event_str);
extern int   METHOD_FN(csprof_ss_started);
extern void  METHOD_FN(csprof_ss_start);

#endif // COMMON_SAMPLE_SOURCE_H
