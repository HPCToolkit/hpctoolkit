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

#include "profargs.hpp"

#include "metricsource.hpp"
#include "finalizers/struct.hpp"
#include "include/hpctoolkit-config.h"

#include <boost/program_options.hpp>
#include <omp.h>
#include <iostream>

using namespace hpctoolkit;
namespace fs = stdshim::filesystem;
namespace po = boost::program_options;

static const std::string version = HPCTOOLKIT_VERSION_STRING;
static const std::string summary =
"[options]... <measurement files/directories>...";
static const std::string description = "\
hpcprof and hpcprof-mpi analyze call path profile performance measurements,\n\
attribute them to static source code structure, and generate an Experiment\n\
database for use with hpcviewer. hpcprof-mpi is a scalable (parallel)\n\
version of hpcprof.\n\
\n\
Both hpcprof and hpcprof-mpi expect a list of measurement-groups, where a\n\
group is a call path profile directory or an individual profile file.\n\
\n\
N.B.: For best results (a) compile your application with debugging\n\
information (e.g., -g); (b) pass recursive search paths with the -I option;\n\
and (c) pass structure information with the -S option.\n\
\n";

static std::string gsub(std::string&& str, std::string pat, std::string rep) {
  std::size_t idx = 0;
  while(true) {
    idx = str.find(pat, idx);
    if(idx == std::string::npos) break;
    str.replace(idx, pat.size(), rep);
    idx += rep.size();
  }
  return str;
}

ProfArgs::ProfArgs(int argc, const char* const argv[]) {
  po::options_description normal("Options");
  po::options_description compat("Depreciated and Superceeded Options");
  po::options_description hidden("Invisible Options");
  normal.add_options()
    ("help,h", "Print this help message.")
    ("help-all", "Print a help message including less common options.")
    ("version,V", "Print the program version and exit.")
    ("verbose,v", "Enable verbose output.\n(Currently unused.)")
    ("max-threads,j", po::value<std::size_t>()->value_name("<threads>")
      ->default_value(1),
      "Number of threads to use during processing.")
    ("output,o", po::value<fs::path>()->value_name("<output>")
      ->default_value("hpctoolkit-database", "hpctoolkit-database"),
      "Specify the database output location.")
    ("format,f", po::value<std::string>()->value_name("<format>")
      ->default_value("exmldb"),
      "Specify the output database format.")
    ("structure,S", po::value<std::vector<fs::path>>()->composing()->value_name("<file>"),
      "Include the given structure file for sample to source correlation.")
    ("replace-path,R", po::value<std::vector<std::string>>()->composing()->value_name("<from>=<to>"),
      "Translate the given prefix when searching for source files "
      "or binaries. Escape `=' with `\\='.")
    ("metric,M", po::value<std::vector<std::string>>()
      ->value_name("(none|<stat>[,<stat>...])")
      ->default_value(std::vector<std::string>{"sum"}, "sum"),
      "Specify global statistics to compute over all threads, from "
      "the following:"
      "\n      sum: \tLinear sum."
      "\n   normal: \tMean and standard deviation"
      "\n  extrema: \tMinimum and maximum"
      "\n    stats: \tsum + normal + extrema")
    ("title,n", po::value<std::string>()->value_name("<name>"),
      "Specify a title for the output database.")
    ("no-thread-local", po::bool_switch(),
      "Disable generation of thread-local statistics.")
    ("no-trace", po::bool_switch(),
      "Disable generation of traces.")
    ("no-source", po::bool_switch(),
      "Disable inclusion of source files.")
    ("never-merge-lines", po::bool_switch(),
      "Generate instruction-level metrics, even if the output format cannot "
      "properly represent them.")
  ;
  compat.add_options()
    ("name", po::value<std::string>()->value_name("<name>"),
      "Superceeded by --title.")
    ("metric-db", po::value<bool>()->value_name("(yes|no)")
      ->default_value(true, "yes"),
      "Superceeded by --no-thread-local.")
    ("include,I", po::value<std::vector<fs::path>>()->composing()->value_name("<path>"),
      "Deprecated, use --replace-path.")
    ("debug", "Deprecated.")
    ("force-metric", "Deprecated.")
    ("remove-dedundancy", "Deprecated.")
    ("struct-id", "Deprecated.")
  ;
  hidden.add_options()
    ("input-path", po::value<std::vector<fs::path>>())
  ;
  po::positional_options_description posopts;
  posopts.add("input-path", -1);
  po::options_description allopts;
  allopts.add(normal).add(compat).add(hidden);

  // Parse the arguments into options
  po::variables_map opts;
  po::store(po::command_line_parser(argc, argv)
            .options(allopts).positional(posopts).run(), opts);
  po::notify(opts);

  // Handle the early-exit options
  if(opts.count("version")) {
    std::cout << version << "\n";
    std::exit(0);
  }
  if(opts.count("help-all")) {
    po::options_description cmdlineopts;
    cmdlineopts.add(normal).add(compat);
    fs::path arg0(argv[0]);
    std::cout << "Usage: " << arg0.filename().string() << " " << summary << "\n"
              << description << cmdlineopts;
    std::exit(0);
  }
  if(opts.count("help")) {
    fs::path arg0(argv[0]);
    std::cout << "Usage: " << arg0.filename().string() << " " << summary << "\n"
              << description << normal;
    std::exit(0);
  }

  // Some bits we don't support, but may (or may not) in the future.
  if(opts.count("verbose"))
    std::cerr << "WARNING: --verbose is not currently supported, there will be "
                 "no difference in output.\n";
  if(opts.count("include")) {
    std::cerr << "--include is not supported, use --replace-path instead!\n";
    std::exit(1);
  /*   for(auto& x: opts["include"].as<std::vector<std::string>>()) */
  /*     includes.emplace_back(std::move(x)); */
  }


  // Easy bits first
  if(opts.count("name") && opts.count("title")) {
    std::cerr << "--name and --title cannot both be specified!\n";
    std::exit(1);
  }
  if(opts.count("name")) title = opts["name"].as<std::string>();
  if(opts.count("title")) title = opts["title"].as<std::string>();
  threads = opts["max-threads"].as<std::size_t>();
  instructionGrain = opts["never-merge-lines"].as<bool>();
  output = opts["output"].as<fs::path>();
  include_sources = !opts["no-source"].as<bool>();
  include_traces = !opts["no-trace"].as<bool>();

  // Slightly complex due to --metric-db compat
  if(!opts["metric-db"].defaulted()) {
    if(!opts["no-thread-local"].defaulted()) {
      std::cerr << "--no-thread-local and --metric-db cannot both be specified!\n";
      std::exit(1);
    }
    include_thread_local = opts["metric-db"].as<bool>();
  } else include_thread_local = !opts["no-thread-local"].as<bool>();

  // Format string -> enum conversion
  {
    auto form = opts["format"].as<std::string>();
    if(form == "exmldb") format = Format::exmldb;
    else {
      std::cerr << "Unrecognized format " << form << "!\n";
      std::exit(1);
    }
  }

  // Prefix replacements
  if(opts.count("replace-path")) {
    for(const auto& x: opts["replace-path"].as<std::vector<std::string>>()) {
      std::size_t idx = 0;
      while(x[idx] != '=') {
        idx = x.find_first_of("\\=", idx);
        if(idx == std::string::npos) {
          std::cerr << "Invalid replace-path argument " << x << ": no separator!\n";
          std::exit(1);
        }
        if(x[idx] == '\\') idx += 2;
      }
      if(idx == 0) {
        std::cerr << "Invalid replace-path argument " << x << ": missing initial prefix!\n";
        std::exit(1);
      }
      fs::path from(gsub(x.substr(0, idx), "\\=", "="));
      fs::path to(gsub(x.substr(idx+1), "\\=", "="));
      if(!prefixes.emplace(std::move(from), std::move(to)).second) {
        std::cerr << "Invalid replace-path argument " << x << ": duplicate replacement for "
          << from << "!\n";
        std::exit(1);
      }
    }
  }

  // Next up is the Sources
  if(!opts.count("input-path")) {
    std::cerr << "No measurement data specified!\n";
    std::exit(1);
  }
  std::unordered_set<fs::path> structs;
  for(const auto& p: opts["input-path"].as<std::vector<fs::path>>()) {
    if(fs::is_directory(p)) {
      bool any = false;
      for(const auto& de: fs::directory_iterator(p)) {
        auto s = MetricSource::create_for(de);
        if(s) { any = true; sources.emplace_back(std::move(s), de.path()); }
      }
      if(!any) {
        std::cerr << "ERROR: No recognized profiles in " << p << "! Aborting!\n";
        std::exit(1);
      }
      // Also check for a structs/ directory for extra structfiles.
      fs::path sp = p / "structs";
      if(fs::exists(sp)) {
        for(const auto& de: fs::directory_iterator(sp)) {
          std::unique_ptr<Finalizer> c;
          try {
            c.reset(new finalizers::StructFile(de));
          } catch(...) { continue; }
          if(structs.emplace(c->filterModule()).second)
            ProfArgs::structs.emplace_back(std::move(c), de);
          else std::cerr << "WARNING: Multiple struct files given for "
                         << c->filterModule().string() << ", using first.\n";
        }
      }
    } else {
      auto s = MetricSource::create_for(p);
      if(!s) {
        std::cerr << "ERROR: Invalid profile " << p << "!\n";
        std::exit(1);
      }
      sources.emplace_back(std::move(s), p);
    }
  }

  // Add Finalizers for the structure files. Sanity-check for duplicates.
  if(opts.count("structure")) {
    for(const auto& path: opts["structure"].as<std::vector<fs::path>>()) {
      std::unique_ptr<Finalizer> c;
      try {
        c.reset(new finalizers::StructFile(std::move(path)));
      } catch(...) { continue; }
      if(structs.emplace(c->filterModule()).second)
        ProfArgs::structs.emplace_back(std::move(c), path);
      else std::cerr << "WARNING: Multiple struct files given for "
                     << c->filterModule().string() << ", using first.\n";
    }
  }
}

static std::pair<bool, fs::path> remove_prefix(const fs::path& path, const fs::path& pre) {
  if(pre.root_path() != path.root_path()) return {false, fs::path()};
  auto rpath = path.relative_path();
  auto rpathit = rpath.begin();
  auto rpathend = rpath.end();
  for(const auto& e: pre.relative_path()) {
    if(rpathit == rpathend) return {false, fs::path()};  // Missing a component
    if(*rpathit != e) return {false, fs::path()};  // Wrong component
    ++rpathit;
  }
  fs::path rem;
  for(; rpathit != rpathend; ++rpathit) rem /= *rpathit;
  return {true, rem};
}

std::vector<fs::path> ProfArgs::prefix(const fs::path& p) const noexcept {
  std::vector<fs::path> paths;
  for(const auto& ft: prefixes) {
    auto xp = remove_prefix(p, ft.first);
    if(xp.first) {
      paths.emplace_back(ft.second / xp.second);
    }
  }
  paths.emplace_back(p);  // If all else fails
  return paths;
}
