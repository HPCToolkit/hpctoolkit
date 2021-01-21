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

#include "tree.hpp"

#include "lib/profile/mpi/all.hpp"

using namespace hpctoolkit;

RankTree::RankTree(std::size_t arity)
  : arity(std::max<std::size_t>(arity, 1)),
    parent(mpi::World::rank() > 0 ? (mpi::World::rank() - 1) / arity : (std::size_t)-1),
    min(mpi::World::rank() * arity + 1),
    max(std::min<std::size_t>(min + arity, mpi::World::size())) {};

Sender::Sender(RankTree& t) : tree(t), stash(nullptr) {};
Sender::Sender(RankTree& t, std::vector<std::uint8_t>& s) : tree(t), stash(&s) {};

void Sender::write() {
  std::vector<std::uint8_t> block;
  packAttributes(block);
  packReferences(block);
  packContexts(block);
  mpi::send(block, tree.parent, 1);

  if(stash) {
    packReferences(*stash);
    packContexts(*stash);
  }
}

Receiver::Receiver(std::size_t p) : peer(p), done(false) {};

void Receiver::read(const DataClass&) {
  if(done) return;
  auto block = mpi::receive_vector<std::uint8_t>(peer, 1);
  iter_t it = block.begin();
  it = unpackAttributes(it);
  it = unpackReferences(it);
  it = unpackContexts(it);
  done = true;
}

void Receiver::append(ProfilePipeline::Settings& pB, RankTree& tree) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++)
    pB << std::make_unique<Receiver>(peer);
}

MetricSender::MetricSender(RankTree& t, bool n) : tree(t), needsTimepoints(n) {};

DataClass MetricSender::accepts() const noexcept {
  using namespace hpctoolkit::literals;
  return data::attributes + data::contexts + data::metrics
         + (needsTimepoints ? data::timepoints : DataClass{});
}

void MetricSender::write() {
  std::vector<std::uint8_t> block;
  packAttributes(block);
  packMetrics(block);
  if(needsTimepoints) packTimepoints(block);
  mpi::send(block, tree.parent, 3);
}

MetricReceiver::MetricReceiver(std::size_t p, ctx_map_t& c)
  : cmap(c), peer(p), done(false), stash(nullptr) {};
MetricReceiver::MetricReceiver(std::size_t p, ctx_map_t& c, const std::vector<std::uint8_t>& s)
  : cmap(c), peer(p), done(false), stash(&s) {};

DataClass MetricReceiver::provides() const noexcept {
  using namespace hpctoolkit::literals;
  return data::attributes + data::metrics + data::timepoints
         + (stash ? data::references + data::contexts : DataClass{});
}

void MetricReceiver::read(const DataClass& d) {
  if(!d.hasMetrics() || done) return;

  if(stash) {
    iter_t it = stash->begin();
    it = unpackReferences(it);
    it = unpackContexts(it);
  }

  auto block = mpi::receive_vector<std::uint8_t>(peer, 3);
  iter_t it = block.begin();
  it = unpackAttributes(it);
  it = unpackMetrics(it, cmap);
  if(sink.limit().hasTimepoints()) it = unpackTimepoints(it);
  done = true;
}

void MetricReceiver::append(ProfilePipeline::Settings& pB, RankTree& tree, ctx_map_t& cmap) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++)
    pB << std::make_unique<MetricReceiver>(peer, cmap);
}
void MetricReceiver::append(ProfilePipeline::Settings& pB, RankTree& tree, ctx_map_t& cmap,
                            const std::vector<std::uint8_t>& stash) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++)
    pB << std::make_unique<MetricReceiver>(peer, cmap, stash);
}
