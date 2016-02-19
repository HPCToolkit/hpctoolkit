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
hpc_init_papi(int (*is_init)(void), int (*init)(int));


/* hpc_init_papi_force: Force initialization of the PAPI library
   perform error checking.  If an error is encountered, print an error
   message and return non-zero; otherwise return 0.  May be called
   only once. */
extern int
hpc_init_papi_force(int (*init)(int));

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
// call to PAPI_sprofil().  Note that the segmented-profile buffers
// will correspond to data in the run-time-load-map.
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

void
dump_hpcpapi_profile_desc(hpcpapi_profile_desc_t* desc, const char* prefix);

void
dump_hpcpapi_profile_desc_buf(hpcpapi_profile_desc_t* desc, int idx, 
			      const char* prefix);


// hpcpapi_profile_desc_vec_t: A vector of hpcpapi_profile_desc_t.
typedef struct {
  int eset; // the event set

  unsigned int            size; // vector size
  hpcpapi_profile_desc_t* vec;  // one for each event
} hpcpapi_profile_desc_vec_t;


void
dump_hpcpapi_profile_desc_vec(hpcpapi_profile_desc_vec_t* descvec);


/****************************************************************************/

#endif /* hpcpapi_h */
