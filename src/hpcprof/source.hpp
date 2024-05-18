// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_SOURCE_H
#define HPCTOOLKIT_PROFILE_SOURCE_H

#include "pipeline.hpp"

#include "util/locked_unordered.hpp"
#include "util/ref_wrappers.hpp"

namespace hpctoolkit {

/// Base class for all sources of metric data. Not to be confused with "sample
/// sources" which are the mechanisms hpcrun uses to get any data at all.
/// Since the rest of the system really doesn't care how the data gets there,
/// but the formats could (potentially) differ widely, this uses a virtual table
/// to hide the difference.
class ProfileSource {
public:
  virtual ~ProfileSource() = default;

  /// Instantiates the proper Source for the given arguments. In time more
  /// overloadings may be added that will handle more interesting cases.
  // MT: Internally Synchronized
  static std::unique_ptr<ProfileSource> create_for(const stdshim::filesystem::path&, const stdshim::filesystem::path&);

  /// Most format errors from a Source can be handled within the Source itself,
  /// but if errors happen during construction callers (create_for) will want to
  /// know. This gives a path for that information.
  // MT: Externally Synchronized
  virtual bool valid() const noexcept;

  /// Bind this Source to a Pipeline to emit the actual bits and bobs.
  // MT: Externally Synchronized
  void bindPipeline(ProfilePipeline::Source&& se) noexcept;

  /// Query what Classes this Source can actually provide to the Pipeline.
  // MT: Safe (const)
  virtual DataClass provides() const noexcept = 0;

  /// Read in enough data to satisfy a request. Unlike the former overload, this
  /// handles cases where the implementation does not obey timeouts, or there is
  /// a better implementation if the limit is not imposed.
  // MT: Externally Synchronized
  virtual void read(const DataClass&) = 0;

  /// In many cases there are dependencies between data reads, due to the nature
  /// of the conversion. This call allows a Source to adjust its request input
  /// based on the dependencies it requires. Note that tracking of previously
  /// requested data is done automatically, so keep this simple and constant.
  virtual DataClass finalizeRequest(const DataClass&) const noexcept = 0;

protected:
  /// Destination for read data. Since Sources may have various needs and orders
  /// for their outputs, they need constant access to a "sink" for whatever they
  /// happen to pull out.
  // MT Internally Synchronized (Implicit)
  ProfilePipeline::Source sink;

  // You should never create a ProfileSource directly
  ProfileSource() = default;
};

}

#endif  // HPCTOOLKIT_PROFILE_SOURCE_H
