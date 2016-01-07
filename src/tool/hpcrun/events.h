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

  extern papi_event_t hpcrun_event_table[];
  
  const papi_event_t *hpcrun_event_by_name(const char *name);
  const papi_event_t *hpcrun_event_by_code(int code);
  
  void hpcrun_write_wrapped_event_list(FILE* fs, const papi_event_t* e);
  void hpcrun_write_event(FILE* fs, const papi_event_t* e);

#ifdef __cplusplus
}
#endif


#endif
