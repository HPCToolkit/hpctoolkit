/* $Id$ */
/* -*-C-*- */

/****************************************************************************
//
// File: 
//    map.h
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

/****************************************************************************/

/* loadmodule_t: runtime load information for a load module */
typedef struct {
  char *name;            /* load module name */
  unsigned long offset;  /* load address or beginning of memory map */
  unsigned long length;  /* length (in bytes) mapped into memory */
} loadmodule_t;

/* loadmodules_t: a vector of loadmodule_t */
typedef struct {
  unsigned int count;   /* vector size */
  loadmodule_t *module; /* the vector */
} loadmodules_t;


#ifdef __cplusplus
extern "C" {
#endif

loadmodules_t* papirun_code_lines_from_loadmap(int dumpmap);

#ifdef __cplusplus
}
#endif

#endif
