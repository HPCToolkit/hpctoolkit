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
// Copyright ((c)) 2002-2022, Rice University
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

// This file defines the API for writing an hpcstruct file directly
// from the TreeNode format.

//***************************************************************************

#ifndef Banal_Struct_Output_hpp
#define Banal_Struct_Output_hpp

#include <list>
#include <ostream>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <lib/support/StringTable.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Skel.hpp"

namespace BAnal {
namespace Output {

class CompactStringTable {
public:
  CompactStringTable() = default;
  ~CompactStringTable() = default;

  // Move-only class. No particular problem with copies, just that you
  // probably don't want to ever do so.
  CompactStringTable(const CompactStringTable&) = delete;
  CompactStringTable& operator=(const CompactStringTable&) = delete;
  CompactStringTable(CompactStringTable&&) = default;
  CompactStringTable& operator=(CompactStringTable&&) = default;

  /// Add a string to the table. Returns the canonical string_view as well as the
  /// final offset into the compact string blob.
  // MT: Internally Synchronized
  std::pair<std::string_view, uint64_t> lookup(std::string_view);

  struct Identifier {
    Identifier(std::string_view string, uint64_t offset)
      : string(string), offset(offset) {};
    std::string_view string;
    uint64_t offset;
  };

  /// Identical to lookup, but the returned value can be injected into a stream.
  // MT: Internally Synchronized
  Identifier lookupId(std::string_view str) {
    auto [s, o] = lookup(str);
    return Identifier(s, o);
  }

  /// Dump the contents of this string table to the given stream. The blob
  /// inserted is arranged such that:
  ///   - every string starts at the offset returned by lookup relative to the
  ///     stream's state prior to this call, and
  ///   - no characters other than those within the strings are inserted.
  /// In particular, this makes it suitable for XML output.
  // MT: Externally Synchronized
  void dump(std::ostream&) const;

private:
  // Storage structure for strings in the table. Like std::vector, but prevents
  // reallocation so the pointers stay valid throughout the lifetime.
  class Blob {
  public:
    explicit Blob(size_t capacity)
      : m_data(new char[capacity]), m_size(0), m_capacity(capacity) {};
    ~Blob() = default;

    template<class It>
    char* insert_back(It first, It last) {
      char* firstOut = &m_data[m_size];
      char* lastOut = std::copy(first, last, firstOut);
      m_size = std::distance(m_data.get(), lastOut);
      assert(m_size <= m_capacity);
      return firstOut;
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }

    void dump(std::ostream&) const;

  private:
    std::unique_ptr<char[]> m_data;
    size_t m_size;
    size_t m_capacity;
  };

  std::shared_mutex mtx;
  // Cache of strings already added to the table. Points into the Blobs.
  std::unordered_map<std::string_view, uint64_t> offsets;
  // Cumulative size of all blobs
  uint64_t totalSize = 0;
  // List of all the blobs, in output order.
  std::list<Blob> blobs;

  // Total number of cache hits
  std::atomic<uint64_t> hits;
};

// Streamification of a CompactStringTable::Identifier for a string. Injects:
//     0x<offset>+<length>
// where both elements are written in hexadecimal.
std::ostream& operator<<(std::ostream&, const CompactStringTable::Identifier&);

// Streamification of a CompactStringTable. See CompactStringTable::dump.
std::ostream& operator<<(std::ostream& os, const CompactStringTable& cst);

void printStructFileBegin(CompactStringTable &, std::ostream *, std::ostream *,
			  std::string);
void printStructFileEnd(CompactStringTable &, std::ostream *, std::ostream *);

void printLoadModuleBegin(CompactStringTable &, std::ostream *, std::string);
void printLoadModuleEnd(CompactStringTable &, std::ostream *);

void printFileBegin(CompactStringTable &, std::ostream *, Struct::FileInfo *);
void printFileEnd(CompactStringTable &, std::ostream *, Struct::FileInfo *);

void printProc(CompactStringTable &, std::ostream *, std::ostream *,
	       std::string, Struct::FileInfo *, Struct::GroupInfo *,
	       Struct::ProcInfo *, HPC::StringTable & strTab);

}  // namespace Output
}  // namespace BAnal

#endif
