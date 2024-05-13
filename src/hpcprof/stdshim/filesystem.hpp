// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_STDSHIM_FILESYSTEM_H
#define HPCTOOLKIT_STDSHIM_FILESYSTEM_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <filesystem>.

#include "version.hpp"

#if defined(HPCTOOLKIT_STDSHIM_STD_HAS_filesystem)
#include <filesystem>

namespace hpctoolkit::stdshim {
namespace filesystem = std::filesystem;
}

#elif defined(HPCTOOLKIT_STDSHIM_STD_HAS_experimental_filesystem)
#include <experimental/filesystem>

namespace hpctoolkit::stdshim {
namespace filesystem {
  class path : public std::experimental::filesystem::path {
  private:
    using base = std::experimental::filesystem::path;

  public:
    template<class... Args>
    path(Args&&... a) : base(std::forward<Args>(a)...) {};

    path relative_path() const { return base::relative_path(); }

    path lexically_normal() const { return (std::string)*this; }
  };

  using std::experimental::filesystem::filesystem_error;
  using std::experimental::filesystem::directory_entry;
  using std::experimental::filesystem::directory_iterator;
  using std::experimental::filesystem::recursive_directory_iterator;
  using std::experimental::filesystem::file_status;
  using std::experimental::filesystem::space_info;
  using std::experimental::filesystem::file_type;
  using std::experimental::filesystem::perms;
  using std::experimental::filesystem::copy_options;
  using std::experimental::filesystem::directory_options;
  using std::experimental::filesystem::file_time_type;

  using std::experimental::filesystem::absolute;
  using std::experimental::filesystem::canonical;
  using std::experimental::filesystem::copy;
  using std::experimental::filesystem::copy_file;
  using std::experimental::filesystem::create_directory;
  using std::experimental::filesystem::create_directories;
  using std::experimental::filesystem::create_hard_link;
  using std::experimental::filesystem::create_symlink;
  using std::experimental::filesystem::create_directory_symlink;
  using std::experimental::filesystem::current_path;
  using std::experimental::filesystem::exists;
  using std::experimental::filesystem::equivalent;
  using std::experimental::filesystem::file_size;
  using std::experimental::filesystem::hard_link_count;
  using std::experimental::filesystem::last_write_time;
  using std::experimental::filesystem::permissions;
  using std::experimental::filesystem::read_symlink;
  using std::experimental::filesystem::remove;
  using std::experimental::filesystem::remove_all;
  using std::experimental::filesystem::rename;
  using std::experimental::filesystem::resize_file;
  using std::experimental::filesystem::space;
  using std::experimental::filesystem::status;
  using std::experimental::filesystem::symlink_status;
  using std::experimental::filesystem::temp_directory_path;

  using std::experimental::filesystem::hash_value;

  using std::experimental::filesystem::is_block_file;
  using std::experimental::filesystem::is_character_file;
  using std::experimental::filesystem::is_directory;
  using std::experimental::filesystem::is_empty;
  using std::experimental::filesystem::is_fifo;
  using std::experimental::filesystem::is_other;
  using std::experimental::filesystem::is_regular_file;
  using std::experimental::filesystem::is_socket;
  using std::experimental::filesystem::is_symlink;
  using std::experimental::filesystem::status_known;
}
}

#else  // HPCTOOLKIT_STDSHIM_STD_HAS_filesystem
#error HPCToolkit requires C++17 std::filesystem support!
#endif  // HPCTOOLKIT_STDSHIM_STD_HAS_filesystem

// C++17 was released without a specialization for std::hash<filesystem::path>.
// This was added back later (GCC 12), so we can't define the specialization
// ourselves. So instead we define the common hash callable.
namespace hpctoolkit::stdshim {
struct hash_path {
  std::size_t operator()(const filesystem::path& p) const noexcept {
    return filesystem::hash_value(p);
  }
};
}  // namespace hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_FILESYSTEM_H
