/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Dynamically link PAPI library.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/************************** System Include Files ****************************/

#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>

/**************************** User Include Files ****************************/

#include "dlpapi.h"

/**************************** Forward Declarations **************************/

dl_PAPI_is_initialized_t dl_PAPI_is_initialized = NULL;

dl_PAPI_library_init_t dl_PAPI_library_init = NULL;

dl_PAPI_get_opt_t dl_PAPI_get_opt = NULL;

dl_PAPI_get_hardware_info_t dl_PAPI_get_hardware_info = NULL;

dl_PAPI_get_event_info_t dl_PAPI_get_event_info = NULL;

dl_PAPI_enum_event_t dl_PAPI_enum_event = NULL;

static void* libpapi = NULL;

/****************************************************************************/

static void handle_any_dlerror();

/* X is the routine name as called from C (i.e. not a string) */
#define CALL_DLSYM(X) \
    dl_ ## X = (dl_ ## X ## _t)dlsym(libpapi, #X)

int
dlopen_papi()
{
  /* Open PAPI lib */
  libpapi = dlopen(HPC_PAPI_LIBSO, RTLD_LAZY);
  handle_any_dlerror();
  
  /* Initialize entry points */
  CALL_DLSYM(PAPI_is_initialized);
  handle_any_dlerror();

  CALL_DLSYM(PAPI_library_init);
  handle_any_dlerror();

  CALL_DLSYM(PAPI_get_opt);
  handle_any_dlerror();

  CALL_DLSYM(PAPI_get_hardware_info);
  handle_any_dlerror();

  CALL_DLSYM(PAPI_get_event_info);
  handle_any_dlerror();

  CALL_DLSYM(PAPI_enum_event);
  handle_any_dlerror();

  return 0;
}

#undef CALL_DLSYM

int 
dlclose_papi()
{
  dlclose(libpapi);
  handle_any_dlerror();
  return 0;
}


static void
handle_any_dlerror()
{
  /* Note: We assume dlsym() or something similar has just been called! */
  char *error;
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error); 
    exit(1);
  }
}


/****************************************************************************/
