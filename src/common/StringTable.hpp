// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
