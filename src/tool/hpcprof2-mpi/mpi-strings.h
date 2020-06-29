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

#ifndef HPCTOOLKIT_PROF2MPI_MPI_STRINGS_H
#define HPCTOOLKIT_PROF2MPI_MPI_STRINGS_H

#include "lib/profile/util/log.hpp"

#include <mpi.h>
#include <vector>
#include <memory>
#include <mutex>
#include <cstring>

static int MPIX_Size(MPI_Comm comm) {
  int sz;
  MPI_Comm_size(comm, &sz);
  return sz;
}

struct ScatterStrings {
  ScatterStrings() : foreach(MPIX_Size(MPI_COMM_WORLD)) {};
  ~ScatterStrings() = default;

  // Append a string to the list for some rank's input.
  void add(int rank, const std::string& str) {
    return add(rank, std::string(str));
  }
  void add(int rank, std::string&& str) {
    foreach.at(rank).emplace_back(std::move(str));
  }

  // Scatter the contents to all our friends. Handles compilation internally.
  // This is the rank 0 version, aka the sender.
  std::vector<std::string> scatter0(int tag) {
    if(foreach.size() == 0) hpctoolkit::util::log::fatal{} << "Attempt to scatter twice!";
    for(std::size_t peer = 1; peer < foreach.size(); peer++) {
      // First stitch together the output block. We use a std::string.
      std::string block;
      for(auto& s: foreach.at(peer)) {
        block += std::move(s);
        block += '\0';  // Explicitly insert a NULL
      }
      // Then send directly to the peer. For now.
      MPI_Send(block.c_str(), block.size(), MPI_CHAR, peer, tag, MPI_COMM_WORLD);
    }
    auto res = std::move(foreach.at(0));
    foreach.clear();  // This is a one-shot.
    return res;
  }

  // Get a scattered content from a friend. Handles decompilation internally.
  // This is the rank N version, aka the reciever.
  static std::vector<std::string> scatterN(int tag) {
    MPI_Message mess;
    MPI_Status stat;
    MPI_Mprobe(0, tag, MPI_COMM_WORLD, &mess, &stat);

    int sz;
    MPI_Get_count(&stat, MPI_CHAR, &sz);
    std::unique_ptr<char[]> block(new char[sz]);
    MPI_Mrecv(block.get(), sz, MPI_CHAR, &mess, &stat);

    std::vector<std::string> res;
    for(int off = 0; off < sz; off += std::strlen(&block[off]) + 1)
      res.emplace_back(&block[off]);
    return res;
  }

  std::vector<std::vector<std::string>> foreach;
};

struct GatherStrings {
  GatherStrings() : contents() {};
  ~GatherStrings() = default;

  // Append a string to the payload
  void add(const std::string& str) {
    return add(std::string(str));
  }
  void add(std::string&& str) {
    contents.emplace_back(std::move(str));
  }

  // Gather the contents from all our friends. Handles decompilation internally.
  // This is the rank 0 version, aka the reciever.
  static std::vector<std::vector<std::string>> gather0(int tag) {
    std::vector<std::vector<std::string>> foreach(MPIX_Size(MPI_COMM_WORLD));
    for(std::size_t peer = 1; peer < foreach.size(); peer++) {
      MPI_Message mess;
      MPI_Status stat;
      MPI_Mprobe(peer, tag, MPI_COMM_WORLD, &mess, &stat);

      int sz;
      MPI_Get_count(&stat, MPI_CHAR, &sz);
      std::unique_ptr<char[]> block(new char[sz]);
      MPI_Mrecv(block.get(), sz, MPI_CHAR, &mess, &stat);

      for(int off = 0; off < sz; off += std::strlen(&block[off]) + 1)
        foreach.at(peer).emplace_back(&block[off]);
    }
    return foreach;
  }

  // Send a gathered content to a friend. Handles compilation internally.
  // This is the rank N version, aka the sender.
  void gatherN(int tag) {
    // First stitch together the output block. We use a std::string.
    std::string block;
    for(auto& s: contents) {
      block += std::move(s);
      block += '\0';  // Explicitly insert a NULL
    }
    // Then send directly to the peer. For now.
    MPI_Send(block.c_str(), block.size(), MPI_CHAR, 0, tag, MPI_COMM_WORLD);
  }

  std::vector<std::string> contents;
};

struct BcastStrings {
  BcastStrings() : contents(), total_size(0) {};
  ~BcastStrings() = default;

  void add(const std::string& str) { return add(std::string(str)); }
  void add(std::string&& str) {
    total_size += str.size() + 1;
    contents.emplace_back(std::move(str));
  }

  std::vector<std::string> bcast0() {
    std::unique_ptr<char[]> block(new char[total_size]);
    int off = 0;
    for(const auto& s: contents) {
      for(const auto& c: s) block[off++] = c;
      block[off++] = '\0';
    }
    MPI_Bcast(&total_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(block.get(), total_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    auto res = std::move(contents);
    return res;
  }

  static std::vector<std::string> bcastN() {
    int sz;
    MPI_Bcast(&sz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::unique_ptr<char[]> block(new char[sz]);
    MPI_Bcast(block.get(), sz, MPI_CHAR, 0, MPI_COMM_WORLD);
    std::vector<std::string> res;
    for(int off = 0; off < sz; off += std::strlen(&block[off]) + 1)
      res.emplace_back(&block[off]);
    return res;
  }

  std::vector<std::string> contents;
  int total_size;
};

template<class T> struct mpi_data {};
template<> struct mpi_data<uint8_t> { static constexpr MPI_Datatype type = MPI_UINT8_T; };
template<> struct mpi_data<uint16_t> { static constexpr MPI_Datatype type = MPI_UINT16_T; };
template<> struct mpi_data<uint32_t> { static constexpr MPI_Datatype type = MPI_UINT32_T; };
template<> struct mpi_data<uint64_t> { static constexpr MPI_Datatype type = MPI_UINT64_T; };
template<> struct mpi_data<double> { static constexpr MPI_Datatype type = MPI_DOUBLE; };

template<class T>
struct Scatter {
  explicit Scatter() : foreach(MPIX_Size(MPI_COMM_WORLD)) {};
  ~Scatter() = default;

  void add(int rank, const T& v) {
    foreach.at(rank).emplace_back(v);
  }

  std::vector<T> scatter0(int tag) {
    if(foreach.size() == 0) hpctoolkit::util::log::fatal{} << "Attempt to scatter twice!";
    for(std::size_t peer = 1; peer < foreach.size(); peer++) {
      auto& fe = foreach.at(peer);
      MPI_Send(fe.data(), fe.size(), mpi_data<T>::type, peer, tag, MPI_COMM_WORLD);
    }
    auto res = std::move(foreach.at(0));
    foreach.clear();
    return res;
  }

  static std::vector<T> scatterN(int tag) {
    MPI_Message mess;
    MPI_Status stat;
    MPI_Mprobe(0, tag, MPI_COMM_WORLD, &mess, &stat);

    int sz;
    MPI_Get_count(&stat, mpi_data<T>::type, &sz);
    std::vector<T> block(sz);
    MPI_Mrecv(block.data(), sz, mpi_data<T>::type, &mess, &stat);
    return block;
  }

  std::vector<std::vector<T>> foreach;
};

template<class T>
struct Gather {
  Gather() : contents() {};
  ~Gather() = default;

  void add(const T& v) { contents.emplace_back(v); }

  static std::vector<std::vector<T>> gather0(int tag) {
    std::vector<std::vector<T>> foreach(MPIX_Size(MPI_COMM_WORLD));
    for(std::size_t peer = 1; peer < foreach.size(); peer++) {
      MPI_Message mess;
      MPI_Status stat;
      MPI_Mprobe(peer, tag, MPI_COMM_WORLD, &mess, &stat);

      int sz;
      MPI_Get_count(&stat, mpi_data<T>::type, &sz);
      std::vector<T> block(sz);
      MPI_Mrecv(block.data(), block.size(), mpi_data<T>::type, &mess, &stat);
      foreach.at(peer) = std::move(block);
    }
    return foreach;
  }

  void gatherN(int tag) {
    MPI_Send(contents.data(), contents.size(), mpi_data<T>::type, 0, tag, MPI_COMM_WORLD);
  }

  std::vector<T> contents;
};

template<class T>
struct Bcast {
  Bcast() : contents() {};
  ~Bcast() = default;

  void add(const T& v) { contents.emplace_back(v); }

  std::vector<T> bcast0() {
    int sz = contents.size();
    MPI_Bcast(&sz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(contents.data(), sz, mpi_data<T>::type, 0, MPI_COMM_WORLD);
    return std::move(contents);
  }

  static std::vector<T> bcastN() {
    int sz;
    MPI_Bcast(&sz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<T> res(sz);
    MPI_Bcast(res.data(), sz, mpi_data<T>::type, 0, MPI_COMM_WORLD);
    return res;
  }

  std::vector<T> contents;
};

template<class T>
void Send(const std::vector<T>& block, int peer, int tag) {
  MPI_Send(block.data(), block.size(), mpi_data<T>::type, peer, tag, MPI_COMM_WORLD);
}

template<class T>
std::vector<T> Receive(int peer, int tag) {
  MPI_Message mess;
  MPI_Status stat;
  MPI_Mprobe(peer, tag, MPI_COMM_WORLD, &mess, &stat);

  int sz;
  MPI_Get_count(&stat, mpi_data<T>::type, &sz);
  std::vector<T> block(sz);
  MPI_Mrecv(block.data(), block.size(), mpi_data<T>::type, &mess, &stat);
  return block;
}

#endif  // HPCTOOLKIT_TPROFMPI_MPI_STRINGS_H
