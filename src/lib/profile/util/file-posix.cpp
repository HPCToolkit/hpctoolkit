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

#define _FILE_OFFSET_BITS 64

#include "file.hpp"

#include "log.hpp"
#include "../mpi/bcast.hpp"

#include <cassert>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace hpctoolkit::util::detail {
struct FileImpl {
  FileImpl(stdshim::filesystem::path path, bool create)
    : path(std::move(path)), create(create) {};
  stdshim::filesystem::path path;
  bool create;
  int fd = -1;
};
struct FileInstanceImpl {
  FileInstanceImpl(int fd) : fd(fd) {};
  int fd;
};
}

using namespace hpctoolkit;
using namespace hpctoolkit::util;

File::File(stdshim::filesystem::path path, bool create) noexcept
  : impl(std::make_unique<detail::FileImpl>(std::move(path), create)) {}
File::~File() = default;

File::File(File&&) = default;
File& File::operator=(File&&) = default;

void File::synchronize() noexcept {
  assert(impl->fd == -1 && "Attempt to call File::synchronize twice!");
  if(mpi::World::rank() == 0) {
    // Check that the file exists, or clear and create if we need to.
    impl->fd = ::open(impl->path.c_str(), O_RDWR | (impl->create ? O_CREAT | O_TRUNC : 0),
                      0666);  // the umask will correct this as needed
    if(impl->fd == -1) {
      char buf[1024];
      util::log::fatal{} << "Error opening file " << impl->path.string()
                         << ": " << strerror_r(errno, buf, sizeof buf);
    }
  }
  mpi::barrier();
  if(mpi::World::rank() != 0) {
    // Open the file that rank 0 has created for us
    impl->fd = ::open(impl->path.c_str(), O_RDWR);
    if(impl->fd == -1) {
      char buf[1024];
      util::log::fatal{} << "Error opening file " << impl->path.string()
                         << ": " << strerror_r(errno, buf, sizeof buf);
    }
  }
}

File::Instance::Instance() = default;
File::Instance::Instance(const File& file, bool writable, bool mapped) noexcept
  : impl(std::make_unique<detail::FileInstanceImpl>(file.impl->fd)) {
  assert(impl->fd != -1 && "Attempt to call File::open before ::synchronize!");
}
File::Instance::~Instance() = default;

File::Instance::Instance(File::Instance&&) = default;
File::Instance& File::Instance::operator=(File::Instance&&) = default;

void File::Instance::readat(std::uint_fast64_t offset, std::size_t size, void* buf) noexcept {
  assert(impl && "Attempt to call readat on an empty File::Instance!");
  auto cnt = pread(impl->fd, buf, size, offset);
  if(cnt < 0) {
    char buf[1024];
    util::log::fatal{} << "Error during read: " << strerror_r(errno, buf, sizeof buf);
  } else if((size_t)cnt < size) {
    util::log::fatal{} << "Error during read: EOF after " << cnt
                       << " bytes (of " << size << " byte read)";
  }
}

void File::Instance::writeat(std::uint_fast64_t offset, std::size_t size, const void* buf) noexcept {
  assert(impl && "Attempt to call writeat on an empty File::Instance!");
  auto cnt = pwrite(impl->fd, buf, size, offset);
  if(cnt < 0) {
    char buf[1024];
    util::log::fatal{} << "Error during write: " << strerror_r(errno, buf, sizeof buf);
  } else if((size_t)cnt < size) {
    util::log::fatal{} << "Error during write: EOF after " << cnt
                       << " bytes (of " << size << " byte write)";
  }
}
