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
   HPCRUN_EVENT_LIST     <event1>:<period1>;<event2>;<event3>:<period3>...
                         (period is optional)
   HPCRUN_OUTPUT_PATH    output file path
   HPCRUN_EVENT_FLAG     a papi sprofil() flag
   HPCRUN_DEBUG          positive integer
 */

#define HPCRUN_VERSION "2.0"

#define HPCRUN_LIB "libhpcrun.so"

// Private debugging level: messages for in-house debugging [0-9]
#define HPCRUN_DBG_LVL 0

#define ERRMSG(...)                                                   \
  { fputs("hpcrun", stderr);                                          \
    if (HPCRUN_DBG_LVL) {                                             \
      fprintf(stderr, " [%s:%d]", __FILE__, __LINE__); }              \
    fprintf(stderr,": (process %d) ", getpid()); fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIE(...) ERRMSG(__VA_ARGS__); { exit(1); }

/**************************** Forward Declarations **************************/

#ifdef MAX
# undef MAX
#endif
#ifdef MIN
# undef MIN
#endif
#define MAX(a,b)	((a>=b)?a:b)
#define MIN(a,b)	((a<=b)?a:b)

/**************************** Forward Declarations **************************/

/* Because these are byte strings, they will not be affected by endianness */

#define HPCRUNFILE_MAGIC_STR "HPCRUN____"
#define HPCRUNFILE_MAGIC_STR_LEN 10  /* exclude '\0' */

#define HPCRUNFILE_VERSION "01.00"
#define HPCRUNFILE_VERSION_LEN 5 /* exclude '\0' */

#define HPCRUNFILE_ENDIAN 'l' /* l for little */

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


/**************************** Forward Declarations **************************/

// hpcrun_ofile_desc_t: Describes an hpcrun output file
typedef struct {
  FILE* fs;    // file stream
  char* fname; // file name
} hpcrun_ofile_desc_t;

/****************************************************************************/

#endif
