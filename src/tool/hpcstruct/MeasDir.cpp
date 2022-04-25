// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

#include "hpcstruct.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include <optional>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <lib/profile/stdshim/filesystem.hpp>
#include <assert.h>
#include <unistd.h>

#include <include/gpu-binary.h>

#include <lib/prof-lean/cpuset_hwthreads.h>
#include <lib/prof-lean/hpcrun-fmt.h>

namespace fs = hpctoolkit::stdshim::filesystem;

// NOTE: fork() doesn't play nicely with OpenMP, so for now the parent process
// doesn't use threading at all.

namespace {
// Structure used to swap out portions of the singleton Args
struct ArgsRestore {
  std::string in_filenm;
  std::string out_filenm;
  int jobs = -1;
  bool cacheonly = false;
  cachestat_t cache_stat = CACHE_MAXENUM;

  void swap(Args& args) {
    std::swap(in_filenm, args.in_filenm);
    std::swap(out_filenm, args.out_filenm);
    std::swap(jobs, args.jobs);
    std::swap(cacheonly, args.cacheonly);
    std::swap(cache_stat, args.cache_stat);
  }
};

// Settings for one of the child worker hpcstruct processes
struct Child {
  fs::path in;
  fs::path out;
  fs::path warnings_out;
  Mode mode;
  unsigned int threads;
  bool cacheonly;

  Child(fs::path xin, fs::path xout, Mode mode, unsigned int threads,
        bool cacheonly)
    : in(std::move(xin)), out(std::move(xout)), mode(mode), threads(threads),
      cacheonly(cacheonly) {
    warnings_out = out;
    warnings_out.replace_extension(".warnings");
  }

  operator ArgsRestore() const {
    ArgsRestore rs;
    rs.in_filenm = in.native();
    rs.out_filenm = out.native();
    rs.jobs = threads;
    rs.cacheonly = cacheonly;
    return rs;
  }
};
} // namespace

using children_t = std::unordered_map<pid_t, Child>;

// Spawn a new worker child, or wait for one of the children to complete
static int async(children_t&, Args& args, const std::string& cache, Child);
static std::pair<int, std::optional<Child>> await_any(children_t&, Args&);

// Signal that cancelled the execution (SIGINT or SIGQUIT)
static int sig_cancelled = 0;
// Set the given handler for SIGINT and SIGQUIT
static void sig_setaction(void (*handler)(int));
// If this process was cancelled, wait for the children and then re-raise the signal
static void sig_cancellation_point(children_t&, Args&);

void
doMeasurementsDir
(
 Args &args,
 const std::string& cache,
 struct stat *sb
)
{
  fs::path measurements_dir = args.in_filenm;
  auto structs_dir = measurements_dir / "structs";

  // Collect the list of *.hpcrun files to parse. Do this first so we can error
  // early if the user didn't give us a measurements dir.
  std::vector<fs::path> m_files;
  {
    std::error_code err;
    for(const fs::path& p: fs::directory_iterator(measurements_dir, err)) {
      if(p.extension() == ".hpcrun")
        m_files.push_back(p);
    }
    if(err) {
      DIAG_EMsg("Unable to open measurements directory " << measurements_dir
                << ": " << err.message());
      exit(1);
    }
  }
  if(m_files.empty()) {
    DIAG_EMsg("Measurements directory " << measurements_dir <<
              "does not contain any .hpcrun measurement files");
    exit(1);
  }

  // Figure out how many threads and jobs are to be used
  unsigned int pthreads;
  unsigned int jobs;

  if (args.jobs == 0) { // not specified
    // Set the default: half the number of CPUs
    unsigned int hwthreads = cpuset_hwthreads();
    jobs = std::max(hwthreads/2, 1U);
    pthreads = std::min(jobs, 16U);
  } else {
    jobs = args.jobs;
    pthreads = jobs;
  }

  // two threads per small binary unless concurrency is 1
  unsigned int small_jobs = (jobs >= 2) ? jobs / 2 : jobs;
  unsigned int small_threads = (jobs == 1) ? 1 : 2;
  unsigned int large_jobs = std::max<unsigned int>(jobs / pthreads, 1);
  unsigned int large_threads = pthreads;

  children_t children;

  std::cerr <<
      "INFO: Using a pool of " << jobs << " threads to analyze binaries in a measurement directory\n"
      "INFO: Analyzing each large binary of >= " << args.parallel_analysis_threshold
        << " bytes in parallel using " << pthreads << " threads\n"
      "INFO: Analyzing each small binary using " << small_threads << " thread"
        << ((small_threads > 1) ? "s" : "") << "\n\n";

  // Create the output structs directory, if it doesn't exist already
  {
    std::error_code err;
    fs::create_directory(structs_dir, err);
    if(err) {
      DIAG_EMsg("Unable to create output directory " << structs_dir.native()
                << ": " << err.message());
      std::exit(1);
    }
  }

  // Parse all the *.hpcrun files and collect a list of binaries
  std::unordered_set<fs::path> lms_set;
  for(size_t i = 0; i < m_files.size(); i++) {
    fs::path m_file = std::move(m_files[i]);
    auto* fs = hpcrun_sparse_open(m_file.c_str(), 0, 0);
    if(fs == nullptr) {
      DIAG_EMsg("Unable to read file " << m_file.native());
      std::exit(1);
    }

    // Read loadmap entries one by one, and collect the ones we want
    int err;
    loadmap_entry_t lm;
    while((err = hpcrun_sparse_next_lm(fs, &lm)) > 0) {
      if((lm.flags & LOADMAP_ENTRY_ANALYZE) == 0) continue;
      fs::path bin = lm.name;

      // This is a binary we want, add it to the list
      lms_set.emplace(std::move(bin));
    }

    hpcrun_sparse_close(fs);
  }
  std::vector<fs::path> lms_q(lms_set.begin(), lms_set.end());
  lms_set.clear();

  // Set up a signal handler for SIGINT and SIGQUIT, so that we wait for our
  // children to complete before dying.
  sig_setaction([](int sig){
    if(sig_cancelled == 0)
      std::cerr << " ...^C or ^\\ received, waiting for workers to close...\n";
    sig_cancelled = sig;
  });

  int ret = 0;
  const auto oneret = [&](int newret){
    if(ret == 0) ret = newret;
  };

  // Do a very fast pre-pass to resolve anything that we can pull from the cache.
  std::vector<fs::path> next_lms_q;
  for(size_t i = 0; i < lms_q.size(); i++) {
    fs::path lm = std::move(lms_q[i]);

    sig_cancellation_point(children, args);

    // Decide if the binary is a GPU binary or not. We do this intellegently by
    // inspecting the extension. Skip it if we aren't supposed to process it.
    bool is_gpu = lm.extension() == GPU_BINARY_SUFFIX;

    if(is_gpu && !args.analyze_gpu_binaries) continue;
    if(!is_gpu && !args.analyze_cpu_binaries) continue;

    // Check that we'll actually be able to process this binary
    std::error_code err;
    auto stat = fs::status(lm, err);
    if(err) {
      DIAG_EMsg("Error while stat'ing " << lm.native() << ": " << err.message());
      continue;
    }
    if(!fs::exists(stat)) {
      // Silently skip over missing binaries
      continue;
    }
    if(!fs::is_regular_file(stat)) {
      DIAG_EMsg("binary " << lm.native() << " is not a file, skipping");
      continue;
    }

    // If we don't have a cache, everything goes into the next rounds
    if(cache.empty()) {
      next_lms_q.emplace_back(std::move(lm));
    } else {
      // Otherwise try pulling just from the cache, which is serial

      // Limit the number of children to the requested value. If the children
      // fail (ie. no cache entry), add them back to the queue for real processing.
      while(children.size() >= jobs) {
        sig_cancellation_point(children, args);
        auto x = await_any(children, args);
        if(x.first != 0 && x.second)
          next_lms_q.emplace_back(std::move(x.second->in));
      }

      sig_cancellation_point(children, args);

      // Spawn a subprocess to check the cache and handle it
      fs::path out = structs_dir / lm.filename();
      out += ".hpcstruct";
      oneret(async(children, args, cache, {lm, out, MODE_STANDARD, 1, true}));
    }
  }
  if(!cache.empty()) {
    // Drain the remaining cache-only workers. If they fail it's back to the queue.
    while(!children.empty()) {
      sig_cancellation_point(children, args);
      auto x = await_any(children, args);
      if(x.first != 0 && x.second)
        next_lms_q.emplace_back(std::move(x.second->in));
    }
  }
  lms_q = std::move(next_lms_q);

  sig_cancellation_point(children, args);

  // Go through and handle all the "large" ones. Along the way build up a queue
  // for the next round, where we process the "small" ones.
  std::vector<fs::path> small_lms_q;
  for(size_t i = 0; i < lms_q.size(); i++) {
    fs::path lm = std::move(lms_q[i]);

    sig_cancellation_point(children, args);

    // If it's too small, defer for the next round.
    std::error_code err;
    auto size = fs::file_size(lm, err);
    if(err) {
      DIAG_EMsg("Error getting size of " << lm.native() << ": " << err.message());
      continue;
    }
    if(size <= (unsigned long)args.parallel_analysis_threshold) {
      small_lms_q.emplace_back(std::move(lm));
      continue;
    }

    // Limit the number of children to keep from blowing out the memory
    while(children.size() >= large_jobs) {
      sig_cancellation_point(children, args);
      oneret(await_any(children, args).first);
    }

    sig_cancellation_point(children, args);

    // Spawn a worker to process it!
    fs::path out = structs_dir / lm.filename();
    out += ".hpcstruct";
    oneret(async(children, args, cache, {lm, out, MODE_STANDARD, large_threads, false}));
  }
  // Drain the workers
  while(!children.empty()) {
    sig_cancellation_point(children, args);
    oneret(await_any(children, args).first);
  }

  sig_cancellation_point(children, args);

  // Now process all the "small" ones. This finishes off the list.
  for(size_t i = 0; i < small_lms_q.size(); i++) {
    fs::path lm = std::move(small_lms_q[i]);

    sig_cancellation_point(children, args);

    // Limit the number of children to keep from blowing out the memory
    while(children.size() >= small_jobs) {
      sig_cancellation_point(children, args);
      oneret(await_any(children, args).first);
    }

    sig_cancellation_point(children, args);

    // Spawn a worker to process it!
    fs::path out = structs_dir / lm.filename();
    out += ".hpcstruct";
    oneret(async(children, args, cache, {lm, out, MODE_CONCURRENT, small_threads, false}));
  }
  // Drain the workers
  while(!children.empty()) {
    sig_cancellation_point(children, args);
    oneret(await_any(children, args).first);
  }

  sig_cancellation_point(children, args);

  // Exit with the appropriate exit code
  std::exit(ret);
}

static const int exitcode_base = 10;

static int async(children_t& children, Args& args, const std::string& cache, Child c) {
  // Stat the file to get it's size for the log message
  struct stat sb;
  if(stat(c.in.c_str(), &sb) != 0) {
    auto e = errno;
    DIAG_EMsg("Failed to stat " << c.in.native() << ": "
              << std::system_category().message(e));
    return 1;
  }

  // Temporarily swap out the arguments for the child execution
  ArgsRestore rs = c;
  rs.cache_stat = args.cache_stat;
  rs.swap(args);
  if(!c.cacheonly)
    printBeginProcess(args, c.mode, &sb);

  // Flush before the fork so the buffers are nice and empty
  std::cerr.flush();
  std::cout.flush();

  // Struct isn't thread-safe, so use a child process to handle the details.
  pid_t pid = fork();
  if(pid < 0) {
    auto e = errno;
    DIAG_EMsg("Failed to fork child process: "
              << std::system_category().message(e));
    return 1;
  }

  if(pid == 0) {
    // Child code. This code should never return.
    try {
      // The child should be using the default signal handling
      sig_setaction(SIG_DFL);

      // FIXME: Worker runs from the directory of the output files, since NVIDIA
      // temporary files use the current working directory.
      if(!c.out.parent_path().empty()) {
        std::error_code ec;
        fs::current_path(c.out.parent_path());
        if(ec) {
          DIAG_EMsg("Failed to chdir to " << c.out.parent_path().native() << ": "
                    << ec.message());
          std::exit(1);
        }

        assert(c.out.parent_path() == c.warnings_out.parent_path());
        c.out = c.out.filename();
        c.warnings_out = c.warnings_out.filename();
      }

      // Open a file on the side for warnings output
      int fd = open(c.warnings_out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if(fd < 0) {
        auto e = errno;
        DIAG_EMsg("Failed to open warnings file " << c.warnings_out.native() << ": "
                  << std::system_category().message(e));
        std::exit(1);
      }
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);

      // Do the actual processing
      doSingleBinary(args, cache, &sb);

      // Check if anything went out to the warnings file. If there wasn't anything
      // delete the empty file (ignoring the error) to prevent confusion.
      bool warnings = true;
      if(c.cacheonly) {
        // Cache-only runs have their warnings disabled
        warnings = false;
        std::error_code ec;
        fs::remove(c.warnings_out, ec);
      } else {
        struct stat wsb;
        if(fstat(fd, &wsb) < 0) {
          auto e = errno;
          DIAG_EMsg("Failed to stat warnings file " << c.warnings_out.native() << ": "
                    << std::system_category().message(e));
        } else if(wsb.st_size == 0) {
          warnings = false;
          std::error_code ec;
          fs::remove(c.warnings_out, ec);
        }
      }

      // Use the exit code to return args.cache_stat and whether there were any warnings
      std::exit(exitcode_base + args.cache_stat + (warnings ? CACHE_MAXENUM : 0));

    // Any exceptions in the child abort the child
    } catch(std::exception& e) {
      DIAG_EMsg("Unhandled exception: " << e.what());
      std::exit(1);
    } catch(...) {
      DIAG_EMsg("Unhandled non-exception error");
      std::exit(1);
    }
  }

  // Restore the arguments in the parent
  rs.swap(args);

  // The parent continues on to other work
  children.emplace(pid, std::move(c));
  return 0;
}

static std::pair<int, std::optional<Child>> await_any(children_t& children, Args& args) {
  // In the parent, wait for a child to complete.
  int status;
  pid_t pid;
  if((pid = wait(&status)) < 0) {
    auto e = errno;
    DIAG_EMsg("Error while waiting for child process: "
              << std::system_category().message(e));
    return {1, {}};
  }

  // We should have no unknown children, but if one pops up, ignore it silently.
  auto it = children.find(pid);
  if(it == children.end()) return {0, {}};
  Child c = std::move(it->second);
  children.erase(it);

  // Log a message about the failure
  if(WIFEXITED(status)) {
    if(exitcode_base <= WEXITSTATUS(status)
       && WEXITSTATUS(status) < exitcode_base + 2*CACHE_MAXENUM) {
      bool warnings = false;
      auto cache_stat = WEXITSTATUS(status) - exitcode_base;
      if(cache_stat >= CACHE_MAXENUM) {
        warnings = true;
        cache_stat -= CACHE_MAXENUM;
      }

      // Report that there were warnings if there were warnings
      if(warnings) {
        std::cerr << "WARNING: incomplete analysis of " << c.in.native()
                  << "; see " << c.warnings_out.native() << " for details\n";
      }

      // Report the end, along with the cache message
      ArgsRestore rs = c;
      rs.cache_stat = (cachestat_t)cache_stat;
      rs.swap(args);

      printEndProcess(args, c.mode);

      rs.swap(args);

      // Worker exited successfully
      return {0, std::move(c)};
    }
    if(!c.cacheonly)
      DIAG_EMsg("hpcstruct " << c.in.native() << " exited with " << WEXITSTATUS(status));
  } else if(WIFSIGNALED(status)) {
    if(!c.cacheonly)
      DIAG_EMsg("hpcstruct " << c.in.native() << " died by signal " << WTERMSIG(status));
  } else {
    if(!c.cacheonly)
      DIAG_EMsg("hpcstruct died by unknown causes");
  }

  // Delete the output file, if one exists
  std::error_code err;
  fs::remove(c.out, err);
  if(err) {
    DIAG_EMsg("Error while removing failed hpcstruct output: " << c.out.native()
              << ": " << err.message());
  }

  return {1, std::move(c)};
}

static void sig_setaction(void (*handler)(int)) {
  struct sigaction sig;
  sig.sa_handler = handler;
  for(int signum: {SIGINT, SIGQUIT}) {
    if(sigaction(signum, &sig, NULL) < 0) {
      auto e = errno;
      DIAG_EMsg("Failed to signal handler for signal " << signum << ": "
                << std::system_category().message(e));
      std::exit(1);
    }
  }
}

static void sig_cancellation_point(children_t& children, Args& args) {
  if(sig_cancelled != 0) {
    while(!children.empty()) await_any(children, args);
    sig_setaction(SIG_DFL);
    raise(sig_cancelled);
  }
}

