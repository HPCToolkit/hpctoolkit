// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "util/vgannotations.hpp"

#include "tree.hpp"

#include "mpi/all.hpp"
#include "util/once.hpp"

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

Receiver::Receiver(std::size_t peer, std::vector<std::uint8_t>& block)
  : peer(peer), block(block) {};
Receiver::Receiver(std::vector<std::uint8_t>& block)
  : readBlock(true), block(block) {};

void Receiver::read(const DataClass& d) {
  if(!readBlock) {
    block = mpi::receive_vector<std::uint8_t>(peer, mpi::Tag::RankTree_1);
    readBlock = true;
  }

  if(!parsedBlock && d.anyOf(provides())) {
    iter_t it = block.begin();
    it = unpackAttributes(it);
    it = unpackReferences(it);
    it = unpackContexts(it);
    parsedBlock = true;
  }
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

void MetricReceiver::read(const DataClass& d) {
  if(!readBlock) {
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
