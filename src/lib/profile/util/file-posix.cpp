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

#include <cstdio>

using namespace hpctoolkit;
using namespace hpctoolkit::util;

struct FileUD {
  FileUD(stdshim::filesystem::path p) : path(std::move(p)) {};
  stdshim::filesystem::path path;
};
template<class T>
static FileUD* udF(T& p) { return (FileUD*)p.get(); }
template<class T>
static const FileUD* udF(const T& p) { return (const FileUD*)p.get(); }

File::File(stdshim::filesystem::path path, bool create) noexcept
  : data(std::make_unique<FileUD>(std::move(path)).release(),
         [](void* d){ delete (FileUD*)d; }) {
  if(mpi::World::rank() == 0) {
    // Check that the file exists, or clear and create if we need to.
    FILE* f = std::fopen(udF(data)->path.c_str(), create ? "w" : "r");
    if(!f) util::log::fatal{} << "Error opening file " << udF(data)->path;
    std::fclose(f);
  }
  mpi::barrier();
}

struct InstanceUD {
  InstanceUD(FILE* p) : filep(p), offset(0) {};
  FILE* filep;
  std::uint_fast64_t offset;
};
template<class T>
static InstanceUD* udI(T& p) { return (InstanceUD*)p.get(); }

File::Instance::Instance(const File& file, bool writable, bool mapped) noexcept {
  FILE* f = std::fopen(udF(file.data)->path.c_str(),
      mapped ? (writable ? "r+bm" : "rbm") : (writable ? "r+b" : "rb"));
  if(!f) util::log::fatal{} << "Error opening file " << udF(file.data);
  data = {std::make_unique<InstanceUD>(f).release(),
          [](void* d){ delete (InstanceUD*)d; }};
}

File::Instance::~Instance() {
  if(!data) return;
  std::fclose(udI(data)->filep);
}

void File::Instance::readat(std::uint_fast64_t offset, std::size_t size, void* buf) noexcept {
  if(!data) util::log::fatal{} << "Attempt to call readat on an empty File::Instance!";
  if(offset != udI(data)->offset)
    fseeko(udI(data)->filep, offset, SEEK_SET);
  auto cnt = std::fread(buf, 1, size, udI(data)->filep);
  udI(data)->offset = offset + cnt;
  if(cnt < size) {
    if(ferror(udI(data)->filep))
      util::log::fatal{} << "Error during read: I/O error";
    else
      util::log::fatal{} << "Error during read: EOF after " << cnt
                         << " bytes (of " << size << " byte read)";
  }
}

void File::Instance::writeat(std::uint_fast64_t offset, std::size_t size, const void* buf) noexcept {
  if(!data) util::log::fatal{} << "Attempt to call writeat on an empty File::Instance!";
  if(offset != udI(data)->offset)
    fseeko(udI(data)->filep, offset, SEEK_SET);
  auto cnt = std::fwrite(buf, 1, size, udI(data)->filep);
  udI(data)->offset = offset + cnt;
  if(cnt < size) {
    if(ferror(udI(data)->filep))
      util::log::fatal{} << "Error during write: I/O error";
    else
      util::log::fatal{} << "Error during write: EOF after " << cnt
                         << " bytes (of " << size << " byte write)";
  }
}
