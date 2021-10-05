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

namespace detail {
struct FileImpl;
struct FileInstanceImpl;
}

/// This represents a file available for access on the filesystem.
class File final {
public:
  class Instance;

  /// Create a File for the given path. May delay the creation of the file until
  /// synchronize().
  /// If `create` is true and the file already exists, it will be cleared.
  File(stdshim::filesystem::path, bool create) noexcept;
  ~File();

  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&&);
  File& operator=(File&&);

  /// Synchronize this File's state between MPI ranks and the filesystem.
  /// This should be called once and only once per File.
  /// Acts as an MPI sychronization point.
  // MT: Externally Synchronized
  void synchronize() noexcept;

  /// Open the File, potentially for write access. Read access is always implied.
  /// Only valid after synchronize() has been called for this File.
  // MT: Internally Synchronized
  Instance open(bool writable, bool mapped) const noexcept {
    return Instance(*this, writable, mapped);
  }

  /// Instance of the opened file, which can be used for file access.
  // MT: Externally Synchronized
  class Instance final {
  public:
    /// Constructs an empty Instance.
    Instance();
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&);
    Instance& operator=(Instance&&);

    /// Read a block of bytes from the given file offset, into the given buffer.
    /// Throws a fatal error on I/O errors.
    void readat(std::uint_fast64_t offset, std::size_t size, void* data) noexcept;

    /// Write a block of bytes at the given offset, from the given buffer.
    /// Throws a fatal error on I/O errors.
    void writeat(std::uint_fast64_t offset, std::size_t size, const void* data) noexcept;

    /// Wrapper for writeat for things like std::array and std::vector
    template<class T>
    void writeat(std::uint_fast64_t offset, const T& data) noexcept {
      return writeat(offset, data.size(), data.data());
    }

  private:
    friend class File;
    Instance(const File&, bool, bool) noexcept;

    std::unique_ptr<detail::FileInstanceImpl> impl;
  };

private:
  std::unique_ptr<detail::FileImpl> impl;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_FILE_H
