#ifndef SAMPLE_SOURCE_H
#define SAMPLE_SOURCE_H

#include "simple_oo.h"

// OO macros for sample_sources

#define METHOD_DEF(retn,name,...) retn (*name)(struct _obj_s *self, ##__VA_ARGS__)

// abbreviation macro for common case of void methods
#define VMETHOD_DEF(name,...) METHOD_DEF(void,name, ##__VA_ARGS__)
#define METHOD_FN(n,...) n(sample_source_t *self, ##__VA_ARGS__)

#include "evlist.h"

// A sample source "state"
// UNINIT and INIT refer to the source
//  START and STOP are on a per-thread basis

typedef enum {
  UNINIT,
  INIT,
  START,
  STOP
} source_state_t;

typedef enum {
  ITIMER,
  PAPI
} sample_source_id_t;

typedef struct _obj_s {
  // common methods

  VMETHOD_DEF(add_event,const char *ev_str);
  VMETHOD_DEF(store_event,int event_id,long thresh);
  METHOD_DEF(char *,get_event_str);
  METHOD_DEF(int,started);

  // specific methods

  VMETHOD_DEF(init);
  VMETHOD_DEF(start);
  VMETHOD_DEF(stop);
  VMETHOD_DEF(shutdown);
  METHOD_DEF(int,supports_event,const char *ev_str);
  VMETHOD_DEF(process_event_list,int lush_agents);
  VMETHOD_DEF(gen_event_set,int lush_agents);

  // data
  evlist_t       evl;       // event list
  int            evset_idx; // index of sample source
  char *         name;      // text name of sample source
  source_state_t state;     // state of sample source: limited to UNINIT or INIT
  
} sample_source_t;


#endif // SAMPLE_SOURCE_H
