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

#ifndef hpcpapi_h
#define hpcpapi_h

/************************** System Include Files ****************************/

#include <papi.h>
#if !(defined(PAPI_VERSION) && (PAPI_VERSION_MAJOR(PAPI_VERSION) >= 3))
# error "Must use PAPI version 3 or greater."
#endif

/**************************** User Include Files ****************************/

/**************************** Forward Declarations **************************/

#ifdef __cplusplus
extern "C" {
#endif

/* hpc_init_papi: Initialize the PAPI library if not already
   initialized and perform error checking.  If an error is
   encountered, print an error message and return non-zero; otherwise
   return 0.  May be called multiple times. */
extern int
hpc_init_papi();

#ifdef __cplusplus
}
#endif

/**************************** Forward Declarations **************************/

typedef struct {
  int code;          /* PAPI flag value */
  const char *name;  /* PAPI flag name */
} papi_flagdesc_t;


#ifdef __cplusplus
extern "C" {
#endif

extern const papi_flagdesc_t *
hpcrun_flag_by_name(const char *name);

#ifdef __cplusplus
}
#endif

/****************************************************************************/

#endif /* hpcpapi_h */
