/* $Id$ */
/* -*-C-*- */

/****************************************************************************
//
// File: 
//    papirun.h
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

/* Because these are byte strings, they will not be affected by endianness */

#define PAPIRUN_MAGIC_STR "HPCRUN____"
#define PAPIRUN_MAGIC_STR_LEN 10  /* exclude '\0' */

#define PAPIRUN_VERSION "01.00"
#define PAPIRUN_VERSION_LEN 5 /* exclude '\0' */

#define PAPIRUN_ENDIAN 'l' /* l for little */

