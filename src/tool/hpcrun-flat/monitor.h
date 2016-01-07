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
//    General header.
//
// Description:
//    Shared declarations, etc for monitoring.
//    
//    *** N.B. *** 
//    There is an automake install hook that will hide these external
//    symbols.
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef hpcrun_flat_monitor_h
#define hpcrun_flat_monitor_h

/************************** System Include Files ****************************/

#include <stdio.h>
#include <unistd.h>      /* getpid() */
#include <inttypes.h>
#include <stdarg.h>

#include <sys/time.h>    /* for sprofil() */
#include <sys/profil.h>  /* for sprofil() */

/**************************** User Include Files ****************************/

#include <include/hpctoolkit-config.h>
#include <include/uint.h>

#include "hpcrun.h"
#include "hpcpapi.h"

/**************************** Forward Declarations **************************/

// Private debugging level: messages for in-house debugging [0-9]
#define HPCRUN_DBG_LVL 0

// MSG should be atomic so that thread messages are not interleaved.
// Because we want to expand the format string with additional output
// parameters, we need to know how to rebuild the fprintf parameters,
// which involves knowing when to include a comma and when not to:
//   "%x1 %x2" fmt, x1, x2   -OR-   "%x1 %x2" fmt, x1, x2, fmt_args
// But doing this requires more compile time evaluation than macros
// support.  Thus, the user must perform this selection by choosing
// between MSG0 and MSGx.

#define MSG_str(fmt)                                                    \
  HPCRUN_NAME" [pid %d, tid 0x%lx]: " fmt "\n", getpid(), hpcrun_gettid()


#define MSG0(x, fmt)                                                    \
  { fprintf(x, MSG_str(fmt)); } 

#define MSGx(x, fmt, ...)                                               \
  { fprintf(x, MSG_str(fmt), __VA_ARGS__); }

/*#define MSG(x, ...)                                                   \
  { fprintf((x), "hpcrun (pid %d, tid 0x%lx): ", getpid(), hpcrun_gettid()); fprintf((x), __VA_ARGS__); fputs("\n", (x)); } */


#define ERRMSG0(fmt)					                \
  { if (HPCRUN_DBG_LVL) {                                               \
      fprintf(stderr, MSG_str("[%s:%d]: " fmt), __FILE__, __LINE__); }	\
    else {								\
      fprintf(stderr, MSG_str(fmt)); }					\
  }

#define ERRMSGx(fmt, ...)					        \
  { if (HPCRUN_DBG_LVL) {                                               \
      fprintf(stderr, MSG_str("[%s:%d]: " fmt), __FILE__, __LINE__, __VA_ARGS__); } \
    else {								\
      fprintf(stderr, MSG_str(fmt), __VA_ARGS__); }			\
  }

/*#define ERRMSG(...)						      \
  { fputs("hpcrun", stderr);                                          \
    if (HPCRUN_DBG_LVL) {                                             \
      fprintf(stderr, " [%s:%d]", __FILE__, __LINE__); }              \
      fprintf(stderr, " (pid %d, tid 0x%lx): ", getpid(), hpcrun_gettid()); fprintf(stderr, __VA_ARGS__); fputs("\n", stderr); } */


#define DIE0(fmt)      ERRMSG0(fmt); { exit(1); }

#define DIEx(fmt, ...) ERRMSGx(fmt, __VA_ARGS__); { exit(1); }

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
  void* papiprofs;  /* hpcpapi_profile_desc_vec_t* */

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

/**************************** Forward Declarations **************************/

/* 'intercepted' libc routines */

#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;
typedef void (*libc_start_main_fini_fptr_t) (void);


#define PARAMS_EXECV  (const char *path, char *const argv[])
#define PARAMS_EXECVP (const char *file, char *const argv[])
#define PARAMS_EXECVE (const char *path, char *const argv[],                 \
                       char *const envp[])

typedef int (*execv_fptr_t)  PARAMS_EXECV;
typedef int (*execvp_fptr_t) PARAMS_EXECVP;
typedef int (*execve_fptr_t) PARAMS_EXECVE;

typedef pid_t (*fork_fptr_t) (void);
typedef void* (*dlopen_fptr_t) (const char *filename, int flag);

typedef void (*_exit_fptr_t) (int);

/* 'intercepted' libpthread routines */

#define PARAMS_PTHREAD_CREATE (pthread_t* thread,                            \
			       const pthread_attr_t* attr,                   \
			       void *(*start_routine)(void*),                \
			       void* arg)

typedef struct {
  void* (*start_routine)(void*);    /* from pthread_create() */
  void* arg;                        /* from pthread_create() */
  hpcrun_profiles_desc_t* profdesc; /* profiling info */
} hpcrun_pthread_create_args_t;


typedef int (*pthread_create_fptr_t) PARAMS_PTHREAD_CREATE;

typedef pthread_t (*pthread_self_fptr_t) (void);


/**************************** Forward Declarations **************************/

#ifdef __cplusplus
extern "C" {
#endif

/* These routines can be called externally. */

extern void init_library();
extern void fini_library();

extern void init_process();
extern void fini_process();

extern hpcrun_profiles_desc_t* 
init_thread(int is_thread);

extern void
fini_thread(hpcrun_profiles_desc_t** profdesc, int is_thread);

extern long hpcrun_gettid();

extern void 
hpcrun_parse_execl(const char*** argv, const char* const** envp,
		   const char* arg, va_list arglist);

extern void handle_dlopen();


/* These routines must be specialized and supplied by someone else.
   monitor.c does _not_ contain their definitions. */

extern void init_library_SPECIALIZED();

extern void init_papi_for_process_SPECIALIZED();

extern long hpcrun_gettid_SPECIALIZED();


#ifdef __cplusplus
}
#endif


/*************************** Variable Declarations **************************/

#ifdef __cplusplus
extern "C" {
#endif


/* hpcrun options: set when the library is initialized */
extern int   opt_debug;
extern int   opt_recursive;
extern hpc_threadprof_t opt_thread;
extern char* opt_eventlist;
extern char  opt_outpath[PATH_MAX];
extern int   opt_flagscode;

/* monitored command: set when library or process is initialized  */
extern const char* hpcrun_cmd; /* profiled command */


#ifdef __cplusplus
}
#endif


/****************************************************************************/

#endif
