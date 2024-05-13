// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
