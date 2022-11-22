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
// Copyright ((c)) 2019-2022, Rice University
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

#include "lib/profile/util/vgannotations.hpp"

#include "tree.hpp"

#include "lib/profile/mpi/all.hpp"
#include "lib/profile/util/once.hpp"

using namespace hpctoolkit;

RankTree::RankTree(std::size_t arity)
  : arity(std::max<std::size_t>(arity, 1)),
    parent(mpi::World::rank() > 0 ? (mpi::World::rank() - 1) / arity : (std::size_t)-1),
    min(mpi::World::rank() * arity + 1),
    max(std::min<std::size_t>(min + arity, mpi::World::size())) {};

Sender::Sender(RankTree& t) : tree(t) {};

void Sender::notifyPipeline() noexcept {
  sinks::Packed::notifyPipeline();
  src.registerOrderedWrite();
}

void Sender::write() {
  std::vector<std::uint8_t> block;
  packAttributes(block);
  packReferences(block);
  packContexts(block);
  {
    auto mpiSem = src.enterOrderedWrite();
    mpi::send(block, tree.parent, mpi::Tag::RankTree_1);
  }
}

Receiver::Receiver(std::size_t peer) : peer(peer) {};
Receiver::Receiver(std::size_t peer, std::vector<std::uint8_t>& blockstore)
  : peer(peer), blockstore(blockstore) {};
Receiver::Receiver(std::vector<std::uint8_t> block)
  : readBlock(true), block(std::move(block)) {};

std::pair<bool, bool> Receiver::requiresOrderedRegions() const noexcept {
  return {!readBlock, false};
}

void Receiver::read(const DataClass& d) {
  if(!readBlock) {
    auto mpiSem = sink.enterOrderedPrewaveRegion();
    block = mpi::receive_vector<std::uint8_t>(peer, mpi::Tag::RankTree_1);
    readBlock = true;
  }

  if(!parsedBlock && d.anyOf(provides())) {
    iter_t it = block.begin();
    it = unpackAttributes(it);
    it = unpackReferences(it);
    it = unpackContexts(it);
    if(blockstore) {
      *blockstore = std::move(block);
    } else {
      block.clear();
    }
    parsedBlock = true;
  }
}

void Receiver::append(ProfilePipeline::Settings& pB, RankTree& tree) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++)
    pB << std::make_unique<Receiver>(peer);
}
void Receiver::append(ProfilePipeline::Settings& pB, RankTree& tree,
    std::deque<std::vector<uint8_t>>& stores) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++) {
    stores.emplace_back();
    pB << std::make_unique<Receiver>(peer, stores.back());
  }
}

MetricSender::MetricSender(RankTree& t)
  : sinks::ParallelPacked(false, true), tree(t) {};

void MetricSender::notifyPipeline() noexcept {
  sinks::ParallelPacked::notifyPipeline();
  src.registerOrderedWrite();
}

void MetricSender::write() {
  std::vector<std::uint8_t> block;
  packMetrics(block);
  packTimepoints(block);
  auto mpiSem = src.enterOrderedWrite();
  mpi::send(block, tree.parent, mpi::Tag::RankTree_2);
}

util::WorkshareResult MetricSender::help() {
  return helpPackMetrics();
}

MetricReceiver::MetricReceiver(std::size_t p, sources::Packed::IdTracker& tracker)
  : sources::Packed(tracker), peer(p) {};

DataClass MetricReceiver::finalizeRequest(const DataClass& d) const noexcept {
  return d;
}

void MetricReceiver::read(const DataClass& d) {
  if(!readBlock) {
    auto mpiSem = sink.enterOrderedPostwaveRegion();
    block = mpi::receive_vector<std::uint8_t>(peer, mpi::Tag::RankTree_2);
    readBlock = true;
  }

  if(!parsedBlock && d.anyOf(provides())) {
    iter_t it = block.begin();
    it = unpackMetrics(it);
    it = unpackTimepoints(it);
    block.clear();
    parsedBlock = true;
  }
}

void MetricReceiver::append(ProfilePipeline::Settings& pB, RankTree& tree,
    hpctoolkit::sources::Packed::IdTracker& tracker) {
  for(std::size_t peer = tree.min; peer < tree.max; peer++)
    pB << std::make_unique<MetricReceiver>(peer, tracker);
}
