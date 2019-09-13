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

#ifndef HPCTOOLKIT_TPROFMPI_MPI_STRINGS_H
#define HPCTOOLKIT_TPROFMPI_MPI_STRINGS_H

#include <mpi.h>

struct ScatterStrings {
  explicit ScatterStrings(int w_size) : foreach(w_size), total_size(0) {};
  ~ScatterStrings() = default;

  // Append a string to the list for some rank's input.
  void add(int rank, const std::string& str) {
    return add(rank, std::string(str));
  }
  void add(int rank, std::string&& str) {
    total_size += str.size() + 1;  // +1 for sentinal \0
    foreach.at(rank).emplace_back(std::move(str));
  }

  // Scatter the contents to all our friends. Handles compilation internally.
  // This is the rank 0 version, aka the sender.
  std::vector<std::string> scatter0() {
    std::vector<int> foreach_sz(foreach.size(), 0);
    std::vector<int> foreach_off(foreach.size(), 0);
    std::unique_ptr<char[]> block(new char[total_size]);
    std::size_t off = 0;
    for(std::size_t r = 1; r < foreach.size(); r++) {  // Skip rank 0, fast-track.
      foreach_off[r] = off;
      for(const auto& s: foreach[r]) {
        foreach_sz[r] += s.size() + 1;
        for(const auto& c: s) block[off++] = c;
        block[off++] = '\0';
      }
    }
    MPI_Scatter(foreach_sz.data(), 1, MPI_INT, MPI_IN_PLACE, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(block.get(), foreach_sz.data(), foreach_off.data(), MPI_CHAR,
      MPI_IN_PLACE, 0, MPI_CHAR, 0, MPI_COMM_WORLD);
    auto res = std::move(foreach[0]);
    foreach.clear();
    total_size = 0;
    return res;
  }

  // Get a scattered content from a friend. Handles decompilation internally.
  // This is the rank N version, aka the reciever.
  static std::vector<std::string> scatterN() {
    int sz;
    MPI_Scatter(nullptr, 1, MPI_INT, &sz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::unique_ptr<char[]> block(new char[sz]);
    MPI_Scatterv(nullptr, nullptr, nullptr, MPI_CHAR,
      block.get(), sz, MPI_CHAR, 0, MPI_COMM_WORLD);
    std::vector<std::string> res;
    for(int off = 0; off < sz; off += std::strlen(&block[off]) + 1)
      res.emplace_back(&block[off]);
    return res;
  }

  std::vector<std::vector<std::string>> foreach;
  std::size_t total_size;
};

struct GatherStrings {
  GatherStrings() : contents(), total_size(0) {};
  ~GatherStrings() = default;

  // Append a string to the payload
  void add(const std::string& str) {
    return add(std::string(str));
  }
  void add(std::string&& str) {
    total_size += str.size() + 1;  // +1 for sentinal \0
    contents.emplace_back(std::move(str));
  }

  // Gather the contents from all our friends. Handles decompilation internally.
  // This is the rank 0 version, aka the reciever.
  static std::vector<std::vector<std::string>> gather0(int w_size) {
    std::vector<int> foreach_sz(w_size, 0);
    std::vector<int> foreach_off;
    MPI_Gather(MPI_IN_PLACE, 1, MPI_INT, foreach_sz.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    int total_size = 0;
    for(const auto& sz: foreach_sz) {
      foreach_off.push_back(total_size);
      total_size += sz;
    }
    std::unique_ptr<char[]> block(new char[total_size]);
    MPI_Gatherv(MPI_IN_PLACE, 0, MPI_CHAR, block.get(), foreach_sz.data(),
      foreach_off.data(), MPI_CHAR, 0, MPI_COMM_WORLD);
    std::vector<std::vector<std::string>> foreach(w_size);
    for(int r = 0; r < w_size; r++)
      for(int off = 0; off < foreach_sz[r]; off += std::strlen(&block[foreach_off[r] + off]) + 1)
        foreach[r].emplace_back(&block[foreach_off[r] + off]);
    return foreach;
  }

  // Send a gathered content to a friend. Handles compilation internally.
  // This is the rank N version, aka the sender.
  void gatherN() {
    std::unique_ptr<char[]> block(new char[total_size]);
    int off = 0;
    for(const auto& s: contents) {
      for(const auto& c: s) block[off++] = c;
      block[off++] = '\0';
    }
    MPI_Gather(&total_size, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gatherv(block.get(), total_size, MPI_CHAR, nullptr, nullptr, nullptr,
      MPI_CHAR, 0, MPI_COMM_WORLD);
  }

  std::vector<std::string> contents;
  int total_size;
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

template<class T>
struct Scatter {
  explicit Scatter(int w_size) : foreach(w_size), total_size(0) {};
  ~Scatter() = default;

  void add(int rank, const T& v) {
    foreach.at(rank).emplace_back(v);
    total_size++;
  }

  std::vector<T> scatter0() {
    std::vector<int> foreach_sz(foreach.size(), 0);
    std::vector<int> foreach_off(foreach.size(), 0);
    std::unique_ptr<T[]> block(new T[total_size]);
    std::size_t off = 0;
    for(std::size_t r = 1; r < foreach.size(); r++) {
      foreach_sz[r] = foreach[r].size();
      foreach_off[r] = off;
      for(const auto& v: foreach[r]) block[off++] = v;
    }
    MPI_Scatter(foreach_sz.data(), 1, MPI_INT, MPI_IN_PLACE, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(block.get(), foreach_sz.data(), foreach_off.data(), mpi_data<T>::type,
      MPI_IN_PLACE, 0, mpi_data<T>::type, 0, MPI_COMM_WORLD);
    auto res = std::move(foreach[0]);
    foreach.clear();
    total_size = 0;
    return res;
  }

  static std::vector<T> scatterN() {
    int sz;
    MPI_Scatter(nullptr, 1, MPI_INT, &sz, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<T> block(sz);
    MPI_Scatterv(nullptr, nullptr, nullptr, mpi_data<T>::type,
      block.data(), sz, mpi_data<T>::type, 0, MPI_COMM_WORLD);
    return block;
  }

  std::vector<std::vector<T>> foreach;
  std::size_t total_size;
};

template<class T>
struct Gather {
  Gather() : contents() {};
  ~Gather() = default;

  void add(const T& v) { contents.emplace_back(v); }

  static std::vector<std::vector<T>> gather0(int w_size) {
    std::vector<int> foreach_sz(w_size, 0);
    std::vector<int> foreach_off;
    MPI_Gather(MPI_IN_PLACE, 1, MPI_INT, foreach_sz.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    int total_size = 0;
    for(const auto& sz: foreach_sz) {
      foreach_off.push_back(total_size);
      total_size += sz;
    }
    std::unique_ptr<T[]> block(new T[total_size]);
    MPI_Gatherv(MPI_IN_PLACE, 0, mpi_data<T>::type, block.get(),
      foreach_sz.data(), foreach_off.data(), mpi_data<T>::type, 0, MPI_COMM_WORLD);
    std::vector<std::vector<T>> foreach(w_size);
    for(int r = 0; r < w_size; r++)
      for(int off = 0; off < foreach_sz[r]; off++)
        foreach[r].emplace_back(block[foreach_off[r] + off]);
    return foreach;
  }

  void gatherN() {
    int sz = contents.size();
    MPI_Gather(&sz, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gatherv(contents.data(), contents.size(), mpi_data<T>::type, nullptr,
      nullptr, nullptr, mpi_data<T>::type, 0, MPI_COMM_WORLD);
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

#endif  // HPCTOOLKIT_TPROFMPI_MPI_STRINGS_H
