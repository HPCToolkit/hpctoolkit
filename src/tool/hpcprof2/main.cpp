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
// Copyright ((c)) 2002-2020, Rice University
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

#include "lib/profile/profile.hpp"
#include "lib/profile/metricsource.hpp"
#include "lib/profile/profilesinks/experimentxml.hpp"
#include "lib/profile/profilesinks/hpctracedb.hpp"
#include "lib/profile/profilesinks/hpcmetricdb.hpp"
#include "lib/profile/finalizers/denseids.hpp"
#include "lib/profile/finalizers/directclassification.hpp"
#include "lib/profile/transformer.hpp"
#include "lib/profile/profargs.hpp"

#include <iostream>

using namespace hpctoolkit;
namespace fs = stdshim::filesystem;

template<class T, class... Args>
static std::unique_ptr<T> make_unique_x(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

int main(int argc, const char* const argv[]) {
  // Read in the arguments.
  ProfArgs args(argc, argv);

  // Get the main core of the Pipeline set up.
  PipelineBuilder pipelineB;
  for(auto& sp : args.sources) pipelineB << std::move(sp.first);
  for(auto& sp : args.structs) pipelineB << std::move(sp.first);

  // Provide Ids for things from the void
  finalizers::DenseIds dids;
  pipelineB << dids;

  // Insert the proper Finalizer for drawing data directly from the Modules.
  finalizers::DirectClassification dc(args);
  pipelineB << dc;

  switch(args.format) {
  case ProfArgs::Format::exmldb: {
    std::unique_ptr<profilesinks::HPCTraceDB> tdb;
    if(args.include_traces)
      tdb = make_unique_x<profilesinks::HPCTraceDB>(args.output, true);
    std::unique_ptr<profilesinks::HPCMetricDB> mdb;
    if(args.include_thread_local)
      mdb = make_unique_x<profilesinks::HPCMetricDB>(args.output);
    auto exml = make_unique_x<profilesinks::ExperimentXML>(args, tdb.get(), mdb.get());
    pipelineB << std::move(tdb) << std::move(mdb) << std::move(exml);

    // ExperimentXML doesn't support instruction-level metrics, so we need
    // a line-merging transformer.
    if(!args.instructionGrain)
      pipelineB << make_unique_x<LineMergeTransformer>();
    break;
  }
  }

  // Now that Modules will be Classified during Finalization, add a Transformer
  // to expand the Contexts as they enter the Pipe.
  ClassificationTransformer ctrans;
  pipelineB << ctrans;

  // Create the Pipeline, let the fun begin.
  ProfilePipeline pipeline(std::move(pipelineB), args.threads);

  // Drain the Pipeline, and make everything happen.
  pipeline.drain();

  return 0;
}
