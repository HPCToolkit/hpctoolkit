/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    General header.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef hpcrun_h
#define hpcrun_h

/************************** System Include Files ****************************/

#include <stdio.h>

/**************************** Forward Declarations **************************/

/* 
   Options to the hpcrun preloaded library (passed via the environment):
   
   Variable              Value
   ---------------------------------------------------------------------
   HPCRUN_RECURSIVE      0 for no; 1 for yes
   HPCRUN_THREAD         see hpc_threadprof_t below
   HPCRUN_EVENT_LIST     <event1>:<period1>;<event2>;<event3>:<period3>...
                         (period is optional)
   HPCRUN_OUTPUT_PATH    output file path
   HPCRUN_EVENT_FLAG     a papi sprofil() flag
   HPCRUN_DEBUG          positive integer
 */

#define HPCRUN_LIB "libhpcrun.so"

typedef enum enum_hpc_threadprof_t {
  HPCRUN_THREADPROF_NO   = 0, /* do not initialize thread support */
#define HPCRUN_THREADPROF_NO_STR   "0"
  
  HPCRUN_THREADPROF_EACH = 1, /* separate profile for each thread */
#define HPCRUN_THREADPROF_EACH_STR "1"
  
  HPCRUN_THREADPROF_ALL  = 2  /* combined profile for all thread */
#define HPCRUN_THREADPROF_ALL_STR  "2"
} hpc_threadprof_t;


/* Special system supported events */

#define HPCRUN_EVENT_WALLCLK_STR     "WALLCLK"
#define HPCRUN_EVENT_WALLCLK_STRLN   7

#define HPCRUN_EVENT_FWALLCLK_STR    "FWALLCLK"
#define HPCRUN_EVENT_FWALLCLK_STRLN  8

/**************************** Forward Declarations **************************/

/* 
   File format:
   
   <header>
   <loadmodule_list>

   ---------------------------------------------------------
   
   <header> ::= <magic string><version><endian>
   <loadmodule_list> ::= 
      <loadmodule_count>
      <loadmodule_1_data>...<loadmodule_n_data>

   <loadmodule_x_data> ::= 
      <loadmodule_name>
      <loadmodule_loadoffset>
      <loadmodule_eventcount>
      <event_1_name>
      <event_1_description>
      <event_1_period>
      <event_1_data>
      ...
      <event_n_name>
      <event_n_description>
      <event_n_period>
      <event_n_data>

   ! A sparse representation of the histogram (only non-zero entries)
   <event_x_data> ::= 
      <histogram_non_zero_bucket_count>
      <histogram_non_zero_bucket_1_value>
      <histogram_non_zero_bucket_1_offset> ! in bytes, from load address
      ...
      <histogram_non_zero_bucket_n_value>
      <histogram_non_zero_bucket_n_offset>
   
   Note: strings are written without null terminators:
      <string_length>
      <string_without_terminator>
 */

/* <header>
   Because these are byte strings, they will not be affected by endianness */

#define HPCRUNFILE_MAGIC_STR "HPCRUN____"
#define HPCRUNFILE_MAGIC_STR_LEN 10  /* exclude '\0' */

#define HPCRUNFILE_VERSION "01.00"
#define HPCRUNFILE_VERSION_LEN 5 /* exclude '\0' */

#define HPCRUNFILE_ENDIAN 'l' /* l for little */

/****************************************************************************/

#endif
