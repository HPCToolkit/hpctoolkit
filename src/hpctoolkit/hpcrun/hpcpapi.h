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

#include <stdlib.h>
#include <inttypes.h>

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


/* hpc_init_papi_force: Force initialization of the PAPI library 
   perform error checking.  If an error is
   encountered, print an error message and return non-zero; otherwise
   return 0.  May be called only once. */
extern int
hpc_init_papi_force();

#ifdef __cplusplus
}
#endif

/**************************** Forward Declarations **************************/

// hpcpapi_flagdesc_t: PAPI_sprofil() flags
typedef struct {
  int code;          /* PAPI flag value */
  const char *name;  /* PAPI flag name */
} hpcpapi_flagdesc_t;


#ifdef __cplusplus
extern "C" {
#endif

extern const hpcpapi_flagdesc_t *
hpcpapi_flag_by_name(const char *name);

#ifdef __cplusplus
}
#endif

/**************************** Forward Declarations **************************/

// hpcpapi_profile_desc_t: Collects all information to describe one
// call to PAPI_sprofil(). 
typedef struct {
  PAPI_event_info_t einfo;       // PAPI's info for the event
  int               ecode;       // PAPI's event code
  uint64_t          period;      // sampling period
  int               flags;       // profiling flags
  
  unsigned int      bytesPerCodeBlk; // bytes per block of monitored code
  unsigned int      bytesPerCntr;    // bytes per histogram counter
  unsigned int      scale;           // relationship between the two
  
  PAPI_sprofil_t*   sprofs;      // vector of histogram buffers, one for each
  unsigned int      numsprofs;   //   run time load module
} hpcpapi_profile_desc_t;

// hpcpapi_profile_desc_vec_t: A vector of hpcpapi_profile_desc_t.
typedef struct {
  int eset; // the event set

  unsigned int            size; // vector size
  hpcpapi_profile_desc_t* vec;  // one for each event
} hpcpapi_profile_desc_vec_t;

/****************************************************************************/

#endif /* hpcpapi_h */
