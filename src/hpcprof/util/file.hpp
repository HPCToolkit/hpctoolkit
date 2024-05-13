// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
  /// This or #initialize should be called once and only once per File.
  /// May act as an MPI synchronization point.
  // MT: Externally Synchronized
  void synchronize() noexcept;

  /// Same as #synchronize, but does not operate across MPI ranks.
  /// The file is assumed to only be accessed by this rank.
  /// This or #synchronize should be called once and only once per File.
  // MT: Externally Synchronized
  void initialize() noexcept;

  /// Delete the File, removing it from the filesystem.
  /// This should be called once and only once per File, after synchronize().
  /// No Instances should be alive during this call, and open() cannot be called
  /// after this function returns.
  /// May act as an MPI synchronization point.
  void remove() noexcept;

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
    void readat(std::uint_fast64_t offset, std::size_t size, char* data) noexcept;

    /// Write a block of bytes at the given offset, from the given buffer.
    /// Throws a fatal error on I/O errors.
    void writeat(std::uint_fast64_t offset, std::size_t size, const char* data) noexcept;

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
