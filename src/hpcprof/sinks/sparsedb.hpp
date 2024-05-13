// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROF2MPI_SPARSE_H
#define HPCTOOLKIT_PROF2MPI_SPARSE_H

#include "../sink.hpp"
#include "../stdshim/filesystem.hpp"
#include "../util/once.hpp"
#include "../util/locked_unordered.hpp"
#include "../util/file.hpp"
#include "../mpi/all.hpp"

#include "../../common/lean/formats/profiledb.h"

#include <vector>

namespace hpctoolkit::sinks {

class SparseDB : public hpctoolkit::ProfileSink {
public:
  SparseDB(hpctoolkit::stdshim::filesystem::path);
  ~SparseDB() = default;

  void write() override;

  hpctoolkit::DataClass accepts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::threads | DataClass::contexts | DataClass::metrics;
  }

  hpctoolkit::DataClass wavefronts() const noexcept override {
    using namespace hpctoolkit;
    return DataClass::contexts + DataClass::threads;
  }

  hpctoolkit::ExtensionClass requirements() const noexcept override {
    using namespace hpctoolkit;
    return ExtensionClass::identifier;
  }

  hpctoolkit::util::WorkshareResult help() override;
  void notifyPipeline() noexcept override;

  void notifyWavefront(hpctoolkit::DataClass) noexcept override;
  void notifyThreadFinal(std::shared_ptr<const hpctoolkit::PerThreadTemporary>) override;

private:
  struct udContext {
    std::atomic<uint64_t> nValues = 0;
    uint16_t nMetrics = 0;
  };
  struct udThread {
    fmt_profiledb_profInfo_t info;
  };
  struct {
    hpctoolkit::Context::ud_t::typed_member_t<udContext> context;
    const auto& operator()(hpctoolkit::Context::ud_t&) const noexcept { return context; }
    hpctoolkit::Thread::ud_t::typed_member_t<udThread> thread;
    const auto& operator()(hpctoolkit::Thread::ud_t&) const noexcept { return thread; }
  } ud;

  // Pre-buffer of Threads that have completed but need to wait before output
  std::shared_mutex prebuffer_lock;
  bool prebuffer_done = false;
  std::vector<std::shared_ptr<const PerThreadTemporary>> prebuffer;

  // Process a Thread and output it's results
  void process(std::shared_ptr<const hpctoolkit::PerThreadTemporary>);

  // Double-buffered concurrent output for profile data, synchronized across
  // multiple MPI ranks.
  class DoubleBufferedOutput {
  public:
    DoubleBufferedOutput();
    ~DoubleBufferedOutput() = default;

    DoubleBufferedOutput(DoubleBufferedOutput&&) = delete;
    DoubleBufferedOutput(const DoubleBufferedOutput&) = delete;
    DoubleBufferedOutput& operator=(DoubleBufferedOutput&&) = delete;
    DoubleBufferedOutput& operator=(const DoubleBufferedOutput&) = delete;

    // Set the output File and the offset data written this way should start at
    // MT: Externally Synchronized
    void initialize(util::File& outfile, uint64_t startOffset);

    // Allocate some space in the file for the given number of bytes, and return
    // the final offset for this space.
    // MT: Internally Synchronized
    uint64_t allocate(uint64_t size);

    // Write a new blob of profile data into the buffer, obtained by
    // concatenating the two given blobs. The final offset of the blob will be
    // saved to `offset` after flush() has been called.
    // MT: Internally Synchronized
    void write(const std::vector<char>& mvBlob, uint64_t& mvOffset,
               const std::vector<char>& ciBlob, uint64_t& ciOffset);

    // Flush the buffers and write everything out to the file.
    // MT: Internally Synchronized
    void flush();

  private:
    // Counter for offsets within the file itself
    mpi::SharedAccumulator pos;
    // File everything gets written to in the end
    util::optional_ref<util::File> file;

    // Lock for the top-level internal state
    std::mutex toplock;
    // Index of the current Buffer to write new data to
    int currentBuf = 0;

    struct Buffer {
      static constexpr size_t bufferSize = 64 * 1024 * 1024;  // 64MiB
      Buffer() { blob.reserve(bufferSize); }

      // Lock for per-Buffer state
      std::mutex lowlock;
      // Blob of buffered data to write out on flush
      std::vector<char> blob;
      // Offsets to update once this Buffer is flushed
      std::vector<std::reference_wrapper<uint64_t>> toUpdate;

      // Flush this Buffer's data to the given File.
      // MT: Externally Synchronized (holding lowlock)
      void flush(util::File& file, uint64_t offset);
    };

    // Buffers to rotate between for parallelism
    std::array<Buffer, 2> bufs;
  } profDataOut;

  // Paths and Files
  std::optional<hpctoolkit::util::File> pmf;
  std::optional<hpctoolkit::util::File> cmf;

  // All the contexts we know about, sorted by identifier.
  // Filled during the Contexts wavefront
  std::deque<std::reference_wrapper<const hpctoolkit::Context>> contexts;

  // Parallel workshares for the various parallel operations
  hpctoolkit::util::ParallelForEach<
      std::reference_wrapper<const Thread>> forEachThread;
  hpctoolkit::util::ParallelFor forProfilesParse;
  hpctoolkit::util::RepeatingParallelFor forProfilesLoad;
  hpctoolkit::util::RepeatingParallelForEach<
      std::pair<uint32_t, uint32_t>> forEachContextRange;
};

}

#endif  // HPCTOOLKIT_PROF2MPI_SPARSE_H
