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

#ifndef support_SrcFile_hpp
#define support_SrcFile_hpp 

//************************** System Include Files ***************************

#include <iostream>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************
// ln: type of a source line number
//***************************************************************************

namespace SrcFile {

  // Use same trick as in OpenAnalysis if there is ambiguity with typedefs
  //   make isValid a member function.
  typedef unsigned int ln;
  const ln ln_NULL = 0;

  inline bool
  isValid(SrcFile::ln line)
  { return (line > ln_NULL); }

  inline bool
  isValid(SrcFile::ln begLine, SrcFile::ln endLine)
  { return (isValid(begLine) && isValid(endLine)); }
  

  // - if x < y; 0 if x == y; + otherwise
  inline int 
  compare(SrcFile::ln x, SrcFile::ln y)
  {
    // The elegant implementation "return (x - y)" may fail since the
    // differences between two 'uints' may be greater than an 'int'
    if (x < y)       { return -1; }
    else if (x == y) { return 0; }
    else             { return 1; }
  }

  // true if src1 includes src2 
  inline bool
  include(SrcFile::ln beg1, SrcFile::ln end1, SrcFile::ln beg2, SrcFile::ln end2)
  {
    return (beg1<=beg2 && end1>=end2); // src1 includes src2 ? 
  }

} // namespace SrcFile


//***************************************************************************
// 
//***************************************************************************

namespace SrcFile {

  // -------------------------------------------------------
  // pos: type of a source position
  // -------------------------------------------------------
  
  //typedef pair<> pos; // line, col
  

} // namespace SrcFile



#endif /* support_SrcFile_hpp */
