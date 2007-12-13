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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <papiStdEventDefs.h>
#include <papi.h>

/**************************** User Include Files ****************************/

#include "hpcpapi.h"

/****************************************************************************/

int
hpc_init_papi(int (*is_init)(void), int (*init)(int))
{
  if ((*is_init)() == PAPI_NOT_INITED) {
    return hpc_init_papi_force(init);
  }
  
  return 0;
}


/****************************************************************************/

int
hpc_init_papi_force(int (*init)(int))
{
  /* Initialize PAPI library */
  int papi_version; 
  papi_version = (*init)(PAPI_VER_CURRENT);
  if (papi_version != PAPI_VER_CURRENT) {
    fprintf(stderr, "(pid %d): PAPI library initialization failure - expected version %d, dynamic library was version %d. Aborting.\n", getpid(), PAPI_VER_CURRENT, papi_version);
    return 1;
  }
  
  if (papi_version < 3) {
    fprintf(stderr, "(pid %d): Using PAPI library version %d; expecting version 3 or greater.\n", getpid(), papi_version);
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
/* No support for dynamically changing bucket size yet...
  { PAPI_PROFIL_BUCKET_16, "PAPI_PROFIL_BUCKET_16" },
  { PAPI_PROFIL_BUCKET_32, "PAPI_PROFIL_BUCKET_32" },
  { PAPI_PROFIL_BUCKET_64, "PAPI_PROFIL_BUCKET_64" }, */
  { PAPI_PROFIL_FORCE_SW, "PAPI_PROFIL_FORCE_SW" },
  { PAPI_PROFIL_DATA_EAR, "PAPI_PROFIL_DATA_EAR" },
  { PAPI_PROFIL_INST_EAR, "PAPI_PROFIL_INST_EAR" },
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


void
dump_hpcpapi_profile_desc_vec(hpcpapi_profile_desc_vec_t* descvec)
{
  int i;

  fprintf(stderr, "{ hpcpapi_profile_desc_vec_t: %d\n", descvec->size);
  for (i = 0; i < descvec->size; ++i) {
    dump_hpcpapi_profile_desc(&descvec->vec[i], "  ");
  }
  fprintf(stderr, "}\n");
}


void
dump_hpcpapi_profile_desc(hpcpapi_profile_desc_t* desc, const char* prefix)
{
  int i;
  const char* pre = (prefix) ? prefix : "  ";
  
  fprintf(stderr, "%s{ %s (%s) : %"PRIu64" : %d entries\n", pre,
	  desc->einfo.symbol, desc->einfo.long_descr, desc->period, 
	  desc->numsprofs);
  
  for (i = 0; i < desc->numsprofs; ++i) {
    dump_hpcpapi_profile_desc_buf(desc, i, "    ");
  }

  fprintf(stderr, "%s}\n", pre);
}


void
dump_hpcpapi_profile_desc_buf(hpcpapi_profile_desc_t* desc, int idx,
			      const char* prefix)
{
  PAPI_sprofil_t* sprof = &(desc->sprofs[idx]);
  uint64_t ncounters = (sprof->pr_size / desc->bytesPerCntr);
  const char* pre = (prefix) ? prefix : "  ";
  fprintf(stderr, "%s[%d](%p): buf %p of size %"PRIu64" for module at %p\n", 
	  pre, idx, sprof, sprof->pr_base, ncounters, sprof->pr_off);
}


/****************************************************************************/
