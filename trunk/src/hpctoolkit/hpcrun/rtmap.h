/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    Finds a list of loaded modules (e.g. DSOs) for the current process.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef map_h
#define map_h

/************************** System Include Files ****************************/

#include <inttypes.h>

/****************************************************************************/

/* rtloadmod_desc_t: runtime load information for a load module */
typedef struct {
  char *name;            /* load module name */
  uint64_t      offset;  /* load address or beginning of memory map */
  unsigned long length;  /* length (in bytes) mapped into memory */
} rtloadmod_desc_t;

/* rtloadmap_t: run time load map (a vector of rtloadmod_desc_t) */
typedef struct {
  unsigned int      count;  /* vector size */
  rtloadmod_desc_t* module; /* the vector */
} rtloadmap_t;


#ifdef __cplusplus
extern "C" {
#endif

extern rtloadmap_t* 
hpcrun_get_rtloadmap(int dbglvl);

extern const char*
hpcrun_get_cmd(int dbglvl);

#ifdef __cplusplus
}
#endif

#endif
