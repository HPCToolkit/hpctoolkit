/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    General PAPI support.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/************************** System Include Files ****************************/

#include <string.h>
#include <stdio.h>
#include <papiStdEventDefs.h>
#include <papi.h>

/**************************** User Include Files ****************************/

#include "hpcpapi.h"

/****************************************************************************/

int
hpc_init_papi()
{
  if (PAPI_is_initialized() == PAPI_NOT_INITED) {
    return hpc_init_papi_force();
  }
  
  return 0;
}


/****************************************************************************/

int
hpc_init_papi_force()
{
  /* Initialize PAPI library */
  int papi_version; 
  papi_version = PAPI_library_init(PAPI_VER_CURRENT);
  if (papi_version != PAPI_VER_CURRENT) {
    fprintf(stderr, "PAPI library initialization failure - expected version %d, dynamic library was version %d. Aborting.\n", PAPI_VER_CURRENT, papi_version);
    return 1;
  }
  
  if (papi_version < 3) {
    fprintf(stderr, "Using PAPI library version %d; expecting version 3 or greater.\n", papi_version);
    return 1;
  }
  return 0;
}

/****************************************************************************/

static hpcpapi_flagdesc_t papi_flags[] = {
  { PAPI_PROFIL_POSIX,    "PAPI_PROFIL_POSIX" },
  { PAPI_PROFIL_RANDOM,   "PAPI_PROFIL_RANDOM" },  
  { PAPI_PROFIL_WEIGHTED, "PAPI_PROFIL_WEIGHTED" },  
  { PAPI_PROFIL_COMPRESS, "PAPI_PROFIL_COMPRESS" },  
  { -1,                   NULL }
}; 

const hpcpapi_flagdesc_t *
hpcpapi_flag_by_name(const char *name)
{
  hpcpapi_flagdesc_t *i = papi_flags;
  for (; i->name != NULL; i++) {
    if (strcmp(name, i->name) == 0) return i;
  }
  return NULL;
}

/****************************************************************************/
