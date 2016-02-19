// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/****************************************************************************
//
// File: 
//    $HeadURL$
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

dl_PAPI_query_event_t dl_PAPI_query_event = NULL;

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
  libpapi = dlopen(HPC_LIBPAPI_SO, RTLD_LAZY);
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

  CALL_DLSYM(PAPI_query_event);
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
