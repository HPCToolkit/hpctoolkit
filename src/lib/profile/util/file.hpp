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
// Copyright ((c)) 2019-2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_UTIL_FILE_H
#define HPCTOOLKIT_PROFILE_UTIL_FILE_H

#include "../stdshim/filesystem.hpp"
#include <functional>
#include <ios>
#include <memory>

namespace hpctoolkit::util {

/// This represents a file available for access on the filesystem.
class File final {
public:
  class Instance;

  /// Create a File for the given path. Acts as an MPI synchronization point.
  /// If `create` is true and the exists, it will be cleared.
  File(stdshim::filesystem::path, bool create) noexcept;
  ~File() = default;

  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&&) = default;
  File& operator=(File&&) = default;

  /// Open the File, potentially for write access. Read access is always implied.
  // MT: Internally Synchronized
  Instance open(bool writable) const noexcept {
    return Instance(*this, writable);
  }

  /// Instance of the opened file, which can be used for file access.
  // MT: Externally Synchronized
  class Instance final {
  public:
    /// Constructs an empty Instance.
    Instance() : data(nullptr) {};
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = default;
    Instance& operator=(Instance&&) = default;

    /// Read a block of bytes from the given file offset, into the given buffer.
    /// Throws a fatal error on I/O errors.
    void readat(std::uint_fast64_t offset, std::size_t size, void* data) noexcept;

    /// Write a block of bytes at the given offset, from the given buffer.
    /// Throws a fatal error on I/O errors.
    void writeat(std::uint_fast64_t offset, std::size_t size, const void* data) noexcept;

  // private:
    friend class File;
    Instance(const File&, bool) noexcept;

    // The actual implementation is decided at link time, so the needed internal
    // size is not known. So we allocate a void* for ease of use.
    std::unique_ptr<void, std::function<void(void*)>> data;
  };

private:
  std::unique_ptr<void, std::function<void(void*)>> data;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_FILE_H
