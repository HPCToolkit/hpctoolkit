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

#ifndef dlpapi_h
#define dlpapi_h

/************************** System Include Files ****************************/

/**************************** User Include Files ****************************/

#include "hpcpapi.h"

/**************************** Forward Declarations **************************/

#ifdef __cplusplus
extern "C" {
#endif

/* Dynamically open and link entry points.  These routines may be
   called multiple times, but the number of closes should equal the
   number of opens. */

extern int dlopen_papi();
extern int dlclose_papi();


/* PAPI entry points */

typedef int (*dl_PAPI_is_initialized_t)(void);
extern dl_PAPI_is_initialized_t dl_PAPI_is_initialized;

typedef int (*dl_PAPI_library_init_t)(int);
extern dl_PAPI_library_init_t dl_PAPI_library_init;

typedef int (*dl_PAPI_get_opt_t)(int, PAPI_option_t*);
extern dl_PAPI_get_opt_t dl_PAPI_get_opt;

typedef const PAPI_hw_info_t* (*dl_PAPI_get_hardware_info_t)(void);
extern dl_PAPI_get_hardware_info_t dl_PAPI_get_hardware_info;

typedef int (*dl_PAPI_get_event_info_t)(int, PAPI_event_info_t*);
extern dl_PAPI_get_event_info_t dl_PAPI_get_event_info;

typedef int (*dl_PAPI_query_event_t)(int);
extern dl_PAPI_query_event_t dl_PAPI_query_event;

typedef int (*dl_PAPI_enum_event_t)(int*, int);
extern dl_PAPI_enum_event_t dl_PAPI_enum_event;

#ifdef __cplusplus
}
#endif

/****************************************************************************/

#endif /* dlpapi_h */
