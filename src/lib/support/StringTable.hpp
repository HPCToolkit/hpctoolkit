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
// Copyright ((c)) 2002-2021, Rice University
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

// This file defines a simple string table to convert between C++
// string and long.  This saves space when there are many copies of
// the same string by storing an index for the string instead of the
// actual string itself.
//
// Notes:
// 1. You can test for string equality by testing their indices.
//
// 2. str2index() inserts the string if not already in the table.
//
// 3. index2str() returns "invalid-string" if the index is out of
// range.  We could possibly throw an exception instead.
//
// 4. This version manages the strings via new and delete.  We could
// add a custom allocator to store the strings in a common area for
// faster bulk deletion.

//***************************************************************************

#ifndef Support_String_Table_hpp
#define Support_String_Table_hpp

#include <map>
#include <string>
#include <vector>

namespace HPC {

// compare the strings, not the pointers
class StringCompare {
public:
  bool operator() (const std::string *s1, const std::string *s2) const
  {
    return *s1 < *s2;
  }
};

class StringTable {
  typedef std::map <const std::string *, long, StringCompare> StringMap;
  typedef std::vector <const std::string *> StringVec;

private:
  StringMap    m_map;
  StringVec    m_vec;
  std::string  m_invalid;

public:
  StringTable()
  {
    m_map.clear();
    m_vec.clear();
    m_invalid = "invalid-string";
  }

  // delete each string individually
  ~StringTable()
  {
    for (long i = 0; i < (long) m_vec.size(); i++) {
      delete m_vec[i];
    }
  }

  // lookup the string in the map and insert if not there
  long str2index(const std::string & str)
  {
    StringMap::iterator it = m_map.find(&str);

    if (it != m_map.end()) {
      return it->second;
    }

    // add string to table
    const std::string *copy = new std::string(str);
    m_vec.push_back(copy);
    long index = m_vec.size() - 1;
    m_map[copy] = index;

    return index;
  }

  const std::string & index2str(long index)
  {
    if (index < 0 || index >= (long) m_vec.size()) {
      return m_invalid;
    }
    return *(m_vec[index]);
  }

  long size()
  {
    return m_vec.size();
  }

};  // class StringTable

}  // namespace HPC

#endif
