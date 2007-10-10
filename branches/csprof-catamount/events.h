/* $Id$ */
/* -*-C-*- */

/****************************************************************************
//
// File: 
//    events.h
//
// Purpose:
//    Supported events for profiling.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (events.h).
//    
*****************************************************************************/

#ifndef _event_h
#define _event_h

typedef struct papi_event {
  int code;                 /* PAPI event code */
  const char *name;         /* PAPI event name */
  const char *description;  /* Event description */
} papi_event_t;


#ifdef __cplusplus
extern "C" {
#endif

  extern papi_event_t csprof_event_table[];
  
  const papi_event_t *csprof_event_by_name(const char *name);
  const papi_event_t *csprof_event_by_code(int code);
  
  void csprof_write_wrapped_event_list(FILE* fs, const papi_event_t* e);
  void csprof_write_event(FILE* fs, const papi_event_t* e);

#ifdef __cplusplus
}
#endif


#endif
