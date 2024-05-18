// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_MPI_ACCUMULATE_H
#define HPCTOOLKIT_PROFILE_MPI_ACCUMULATE_H

#include "core.hpp"

#include <atomic>
#include <memory>


namespace hpctoolkit::mpi {

namespace detail {
  struct Win;
}  // namespace detail

class SharedAccumulator {
public:
  SharedAccumulator(Tag tag);
  ~SharedAccumulator();


  void initialize(std::uint64_t init);
  std::uint64_t fetch_add(std::uint64_t val);

private:
  std::atomic<std::uint64_t> atom;
  std::unique_ptr<detail::Win> detail;
};


}  // namespace hpctoolkit::mpi

#endif  // HPCTOOLKIT_PROFILE_MPI_ACCUMULATE_H
