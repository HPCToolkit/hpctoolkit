// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "util/vgannotations.hpp"

#include "args.hpp"

#include "pipeline.hpp"
#include "source.hpp"
#include "sinks/hpctracedb2.hpp"
#include "sinks/metadb.hpp"
#include "sinks/metricsyaml.hpp"
#include "sinks/sparsedb.hpp"
#include "finalizers/denseids.hpp"
#include "finalizers/directclassification.hpp"
#include "finalizers/logical.hpp"

#include <memory>
#include <iostream>

using namespace hpctoolkit;
namespace fs = stdshim::filesystem;

int main(int argc, char* const argv[]) {
  // Read in the arguments.
  ProfArgs args(argc, argv);

  // Get the main core of the Pipeline set up.
  ProfilePipeline::Settings pipelineB;
  for(auto& sp : args.sources) pipelineB << std::move(sp.first);
  ProfArgs::StatisticsExtender se(args);
  pipelineB << se;

  // Provide Ids for things from the void
  finalizers::DenseIds dids;
  pipelineB << dids;

  // Make sure the files are searched for as they should be
  ProfArgs::Prefixer pr(args);
  pipelineB << pr;

  // Load in the Finalizers for special cases
  finalizers::LogicalFile lf;
  pipelineB << lf;
  for(auto& sp : args.ksyms) pipelineB << std::move(sp.first);

  // Load in the Finalizers for Structfiles
  for(auto& sp : args.structs) pipelineB << std::move(sp.first);
  ProfArgs::StructPartialMatch spm(args);
  pipelineB << spm;

  // Insert the proper Finalizer for drawing data directly from the Modules.
  // This is used as a fallback if the Structfiles aren't available.
  pipelineB << std::make_unique<finalizers::DirectClassification>(args.dwarfMaxSize);

  switch(args.format) {
  case ProfArgs::Format::metadb: {
    pipelineB << std::make_unique<sinks::MetaDB>(args.output, args.include_sources)
              << std::make_unique<sinks::SparseDB>(args.output)
              << std::make_unique<sinks::MetricsYAML>(args.output);
    if(args.include_traces)
      pipelineB << std::make_unique<sinks::HPCTraceDB2>(args.output);
    break;
  }
  }

  // Create the Pipeline, let the fun begin.
  ProfilePipeline pipeline(std::move(pipelineB), args.threads);

  // Drain the Pipeline, and make everything happen.
  pipeline.run();

  if(args.valgrindUnclean) std::exit(0);  // Skips local cleanup of pipeline

  return 0;
}
