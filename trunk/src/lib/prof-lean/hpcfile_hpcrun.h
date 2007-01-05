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

#ifndef hpcfile_hpcrun_h
#define hpcfile_hpcrun_h

/************************** System Include Files ****************************/

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
