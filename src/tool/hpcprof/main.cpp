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

#include "args.hpp"

#include "lib/profile/pipeline.hpp"
#include "lib/profile/source.hpp"
#include "lib/profile/sinks/hpctracedb2.hpp"
#include "lib/profile/sinks/metadb.hpp"
#include "lib/profile/sinks/metricsyaml.hpp"
#include "lib/profile/sinks/sparsedb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/finalizers/logical.hpp"

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
  for(auto& sp : args.ksyms) pipelineB << std::move(sp.first);
  ProfArgs::StatisticsExtender se(args);
  pipelineB << se;

  // Provide Ids for things from the void
  finalizers::DenseIds dids;
  pipelineB << dids;

  // Make sure the files are searched for as they should be
  ProfArgs::Prefixer pr(args);
  pipelineB << pr;

  // Load in the Finalizer to parse logical constructs
  finalizers::LogicalFile lf;
  pipelineB << lf;

  // Load in the Finalizers for Structfiles
  for(auto& sp : args.structs) pipelineB << std::move(sp.first);
  ProfArgs::StructPartialMatch spm(args);
  pipelineB << spm;

  if(!args.foreign) {
    // Insert the proper Finalizer for drawing data directly from the Modules.
    // This is used as a fallback if the Structfiles aren't available.
    pipelineB << std::make_unique<finalizers::DirectClassification>(
        args.dwarfMaxSize, !args.structs.empty());
  }

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
