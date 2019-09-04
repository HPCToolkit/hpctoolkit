// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2019, Rice University
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

//***************************************************************************
//
// File:
//   StringSet.cpp
//
// Purpose:
//   implement I/O for StringSet abstraction used by hpcprof/hpcprof-mpi
//
// Description:
//   fmt_fread: read a string set from a file stream
//   fmt_fwrite: write a string set to a file stream
//
//***************************************************************************

//***************************************************************************
// system include files
//***************************************************************************

#include <iostream>



//***************************************************************************
// local include files
//***************************************************************************

#include "StringSet.hpp"
#include <lib/prof-lean/hpcfmt.h>



//***************************************************************************
// interface functions
//***************************************************************************

int
StringSet::fmt_fread
(
 StringSet* &stringSet,  // result set read from stream
 FILE* infs                    // input file stream
)
{
  int result; 

  stringSet = new StringSet;

  // ------------------------------------------------------------------
  // read string set size
  // ------------------------------------------------------------------
  uint64_t size = 0;

  result = hpcfmt_int8_fread(&size, infs);
  if (result != HPCFMT_OK) return result;

  // ------------------------------------------------------------------
  // read strings in string set
  // ------------------------------------------------------------------
  for (size_t i = 0; i < size; i++) {
    char *cstr = NULL;

    int result = hpcfmt_str_fread(&cstr, infs, malloc);
    if (result != HPCFMT_OK) return result;

    std::string str = cstr;
    free(cstr);

    stringSet->insert(str);
  }

  return HPCFMT_OK;
}


int
StringSet::fmt_fwrite
(
 const StringSet& stringSet,  // input set to write to stream
 FILE* outfs                        // output file stream
)
{
  int result;

  // ------------------------------------------------------------------
  // write string set size
  // ------------------------------------------------------------------
  uint64_t size = stringSet.size();

  result = hpcfmt_int8_fwrite(size, outfs);
  if (result != HPCFMT_OK) return HPCFMT_ERR;

  // ------------------------------------------------------------------
  // write strings in string set
  // ------------------------------------------------------------------
  for (auto s = stringSet.begin(); s != stringSet.end(); s++) {
    result = hpcfmt_str_fwrite((*s).c_str(), outfs);
    if (result != HPCFMT_OK) return HPCFMT_ERR;
  }

  return HPCFMT_OK;
}


void
StringSet::dump
(
 void
)
{
  // ------------------------------------------------------------------
  // dump strings in string set
  // ------------------------------------------------------------------
  std::cerr << "StringSet (" << this << ")" << std::endl;
  for (auto s = begin(); s != end(); s++) {
    std::cerr << "\t" << *s << std::endl;
  }

}
