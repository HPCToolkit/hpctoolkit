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

#define HPCRUN_VERSION "2.0"

#define HPCRUN_LIB "libhpcrun.so"

/**************************** Forward Declarations **************************/

/* Because these are byte strings, they will not be affected by endianness */

#define HPCRUNFILE_MAGIC_STR "HPCRUN____"
#define HPCRUNFILE_MAGIC_STR_LEN 10  /* exclude '\0' */

#define HPCRUNFILE_VERSION "01.00"
#define HPCRUNFILE_VERSION_LEN 5 /* exclude '\0' */

#define HPCRUNFILE_ENDIAN 'l' /* l for little */

/**************************** Forward Declarations **************************/

// hpcrun_ofile_desc_t: Describes an hpcrun output file
typedef struct {
  FILE* fs;    // file stream
  char* fname; // file name
} hpcrun_ofile_desc_t;

// hpcrun_ofile_desc_vec_t: A vector of hpcrun_ofile_desc_t.
typedef struct {
  unsigned int         size; /* vector size */
  hpcrun_ofile_desc_t* vec;  /* the vector */
} hpcrun_ofile_desc_vec_t;

/****************************************************************************/

#endif
