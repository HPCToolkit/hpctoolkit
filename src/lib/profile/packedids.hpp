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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_PACKEDIDS_H
#define HPCTOOLKIT_PROFILE_PACKEDIDS_H

#include "transformer.hpp"
#include "sink.hpp"
#include "finalizer.hpp"
#include "sources/packed.hpp"

namespace hpctoolkit {

class IdPacker final {
public:
  IdPacker();
  ~IdPacker() = default;

  class Classifier final : public ClassificationTransformer {
  public:
    Classifier(IdPacker& s) : shared(s) {};
    ~Classifier() = default;

    Context& context(Context&, Scope&) override;

  private:
    IdPacker& shared;
  };

  class Sink : public ProfileSink {
  public:
    Sink(IdPacker& s);

    DataClass accepts() const noexcept override {
      return DataClass::references + DataClass::contexts;
    }
    ExtensionClass requires() const noexcept override {
      return ExtensionClass::identifier;
    }
    DataClass wavefronts() const noexcept override {
      return DataClass::references + DataClass::contexts;
    }

    void notifyWavefront(DataClass::singleton_t) override;
    void write() override {};

  protected:
    virtual void notifyPacked(std::vector<uint8_t>&&) = 0;

  private:
    IdPacker& shared;
    std::atomic<int> wave;
  };

private:
  std::mutex ctxtree_m;
  std::unordered_map<Context*, std::unordered_set<Scope>> ctxseen;
  std::size_t ctxcnt;
  std::vector<uint8_t> ctxtree;
};

class IdUnpacker final {
public:
  IdUnpacker(std::vector<uint8_t>&&);
  ~IdUnpacker() = default;

  class Expander final : public ProfileTransformer {
  public:
    Expander(IdUnpacker& s) : shared(s) {};
    ~Expander() = default;

    Context& context(Context&, Scope&) override;

  private:
    void unpack();
    IdUnpacker& shared;
  };

  class Finalizer final : public ProfileFinalizer {
  public:
    Finalizer(IdUnpacker& s) : shared(s) {};
    ~Finalizer() = default;

    ExtensionClass provides() const noexcept override { return ExtensionClass::identifier; }
    ExtensionClass requires() const noexcept override { return {}; }

    void context(const Context&, unsigned int&) override;

  private:
    IdUnpacker& shared;
  };

private:
  std::vector<uint8_t> ctxtree;

  std::once_flag once;
  const Module* exmod;
  const File* exfile;
  std::unique_ptr<Function> exfunc;
  std::vector<std::reference_wrapper<const Module>> modmap;
  unsigned int globalid;
  std::unordered_map<unsigned int, std::unordered_map<Scope, std::vector<Scope>>> exmap;
};

}

#endif  // HPCTOOLKIT_PROFILE_PACKEDIDS_H
