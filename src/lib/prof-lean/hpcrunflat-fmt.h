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
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef prof_lean_hpcfile_hpcrun_h
#define prof_lean_hpcfile_hpcrun_h

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

#define HPCRUNFLAT_FMT_Magic "HPCRUN-FLAT_______"
#define HPCRUNFLAT_FMT_MagicLen 18  /* exclude '\0' */

#define HPCRUNFLAT_Version "01.00"
#define HPCRUNFLAT_VersionLen 5 /* exclude '\0' */

#define HPCRUNFLAT_FMT_Endian 'l' /* l for little */

/****************************************************************************/

#endif /* prof_lean_hpcfile_hpcrun_h */
