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
#include <inttypes.h>

#include <sys/time.h>    /* for sprofil() */
#include <sys/profil.h>  /* for sprofil() */

#include "hpcpapi.h"

/**************************** Forward Declarations **************************/

/* 
   Options to the hpcrun preloaded library (passed via the environment):
   
   Variable              Value
   ---------------------------------------------------------------------
   HPCRUN_RECURSIVE      0 for no; 1 for yes
   HPCRUN_THREAD         0 for no; 1 for yes
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
    fprintf(stderr," (pid %d): ", getpid()); fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); }

#define DIE(...) ERRMSG(__VA_ARGS__); { exit(1); }

/**************************** Forward Declarations **************************/

/* Special system supported events */

#define HPCRUN_EVENT_WALLCLK_STR     "WALLCLK"
#define HPCRUN_EVENT_WALLCLK_STRLN   7

#define HPCRUN_EVENT_FWALLCLK_STR    "FWALLCLK"
#define HPCRUN_EVENT_FWALLCLK_STRLN  8

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

// hpcsys_profile_desc_t: Collects all information to describe system
// based (i.e. non-PAPI) profiles, e.g. a call to sprofil(). Note that
// the segmented-profile buffers will correspond to data in the
// run-time-load-map.
typedef struct {
  /* currently we only have one type of system prof.  If we ever need
     more we can add a prof-type field */ 
  char*             ename;       // event name
  uint64_t          period;      // sampling period
  //struct timeval*   tval;      // contains info after a call to sprofil()
  unsigned int      flags;       // profiling flags
  
  unsigned int      bytesPerCodeBlk; // bytes per block of monitored code
  unsigned int      bytesPerCntr;    // bytes per histogram counter
  unsigned int      scale;           // relationship between the two
  
  struct prof*      sprofs;      // vector of histogram buffers, one for each
  unsigned int      numsprofs;   //   run time load module
} hpcsys_profile_desc_t;

// hpcsys_profile_desc_vec_t: A vector of hpcsys_profile_desc_t.
typedef struct {
  unsigned int           size; // vector size
  hpcsys_profile_desc_t* vec;  // one for each profile
  
} hpcsys_profile_desc_vec_t;

/**************************** Forward Declarations **************************/

// hpcrun_ofile_desc_t: Describes an hpcrun output file
typedef struct {
  FILE* fs;    // file stream
  char* fname; // file name
} hpcrun_ofile_desc_t;


// hpcrun_profiles_desc_t: Describes all concurrent profiles for a
// particular process or thread.  
typedef struct {
  /* We use void* to make conditional compilation easy.  See macros below. */
  void* sysprofs;   /* hpcsys_profile_desc_vec_t* */
  void* papiprofs;  /* hpcpapi_profile_desc_vec_t */

  hpcrun_ofile_desc_t ofile; 
} hpcrun_profiles_desc_t;

/* Each accessor macro has two versions, one for use as lvalue and
   rvalue.  The reason is that casts in lvalue expressions is a
   non-standard. */
#define HPC_GETL_SYSPROFS(x)  ((x)->sysprofs)
#define HPC_GET_SYSPROFS(x)  ((hpcsys_profile_desc_vec_t*)((x)->sysprofs))

//#if HAVE_PAPI
#define HPC_GETL_PAPIPROFS(x) ((x)->papiprofs)
#define HPC_GET_PAPIPROFS(x) ((hpcpapi_profile_desc_vec_t*)((x)->papiprofs))
//#else
//#define HPC_GET_PAPIPROFS(x) (x->papiprofs)
//#endif

/****************************************************************************/

#endif
