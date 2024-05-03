// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
File::~File() {
  if(impl && impl->fd != -1)
    close(impl->fd);
}

File::File(File&&) = default;
File& File::operator=(File&&) = default;

void File::synchronize() noexcept {
  assert(impl && "Attempt to call File::synchronize after ::remove!");
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

void File::initialize() noexcept {
  assert(impl && "Attempt to call File::synchronize after ::remove!");
  assert(impl->fd == -1 && "Attempt to call File::synchronize twice!");
  // Check that the file exists, or clear and create if we need to.
  impl->fd = ::open(impl->path.c_str(), O_RDWR | (impl->create ? O_CREAT | O_TRUNC : 0),
                    0666);  // the umask will correct this as needed
  if(impl->fd == -1) {
    char buf[1024];
    util::log::fatal{} << "Error opening file " << impl->path.string()
                        << ": " << strerror_r(errno, buf, sizeof buf);
  }
}


void File::remove() noexcept {
  assert(impl && impl->fd != -1 && "Attempt to call File::remove before File::synchronize!");
  if(mpi::World::rank() == 0) {
    stdshim::filesystem::remove(impl->path);
  }
  close(impl->fd);
  impl.reset();
}

File::Instance::Instance() = default;
File::Instance::Instance(const File& file, bool writable, bool mapped) noexcept
  : impl(std::make_unique<detail::FileInstanceImpl>(file.impl->fd)) {
  assert(impl && "Attempt to call File::open after ::remove!");
  assert(impl->fd != -1 && "Attempt to call File::open before ::synchronize!");
}
File::Instance::~Instance() = default;

File::Instance::Instance(File::Instance&&) = default;
File::Instance& File::Instance::operator=(File::Instance&&) = default;

void File::Instance::readat(std::uint_fast64_t offset, std::size_t size, char* buf) noexcept {
  assert(impl && "Attempt to call readat on an empty File::Instance!");
  const auto orig_size = size;
  while(size > 0) {
    auto cnt = pread(impl->fd, buf, size, offset);
    if(cnt < 0) {
      char buf[1024];
      util::log::fatal{} << "Error during read: " << strerror_r(errno, buf, sizeof buf);
    } else if(cnt == 0) {
      util::log::fatal{} << "Error during read: EOF after " << (orig_size - size)
                        << " bytes (of " << orig_size << " byte read)";
    }

    // Adjust the arguments for the next time attempt
    offset += cnt;
    size -= cnt;
    buf += cnt;
  }
}

void File::Instance::writeat(std::uint_fast64_t offset, std::size_t size, const char* buf) noexcept {
  assert(impl && "Attempt to call writeat on an empty File::Instance!");
  const auto orig_size = size;
  while(size > 0) {
    auto cnt = pwrite(impl->fd, buf, size, offset);
    if(cnt < 0) {
      char buf[1024];
      util::log::fatal{} << "Error during write: " << strerror_r(errno, buf, sizeof buf);
    } else if(cnt == 0) {
      util::log::fatal{} << "Error during write: EOF after " << (orig_size - size)
                        << " bytes (of " << orig_size << " byte write)";
    }

    // Adjust the arguments for the next time attempt
    offset += cnt;
    size -= cnt;
    buf += cnt;
  }
}
