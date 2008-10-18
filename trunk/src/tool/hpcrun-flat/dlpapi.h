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

typedef int (*dl_PAPI_enum_event_t)(int*, int);
extern dl_PAPI_enum_event_t dl_PAPI_enum_event;

#ifdef __cplusplus
}
#endif

/****************************************************************************/

#endif /* dlpapi_h */
