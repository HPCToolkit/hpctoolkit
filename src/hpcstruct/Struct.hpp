// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file defines the external API that Struct.cpp provides for
// tool/hpcstruct/main.cpp.

//***************************************************************************

#ifndef BAnal_Struct_hpp
#define BAnal_Struct_hpp

#include <ostream>
#include <string>

namespace BAnal {
namespace Struct {

// Parameters on how to run makeStructure().
class Options {
public:
  unsigned int jobs;

  unsigned int jobs_struct;
  unsigned int jobs_parse;
  unsigned int jobs_symtab;

  bool show_time;

  bool analyze_cpu_binaries;

  bool analyze_gpu_binaries;
  bool compute_gpu_cfg;

  unsigned long parallel_analysis_threshold;

  bool pretty_print_output;

  void set
  (
   unsigned int _jobs,
   unsigned int _jobs_struct,
   unsigned int _jobs_parse,
   unsigned int _jobs_symtab,
   bool _show_time,
   bool _analyze_cpu_binaries,
   bool _analyze_gpu_binaries,
   bool _compute_gpu_cfg,
   unsigned long _parallel_analysis_threshold,
   bool _pretty_print_output
  ) {
   jobs = _jobs;
   jobs_struct = _jobs_struct;
   jobs_parse  = _jobs_parse;
   jobs_symtab = _jobs_symtab;
   show_time = _show_time;
   analyze_cpu_binaries = _analyze_cpu_binaries;
   analyze_gpu_binaries = _analyze_gpu_binaries;
   compute_gpu_cfg = _compute_gpu_cfg;
   parallel_analysis_threshold = _parallel_analysis_threshold;
   pretty_print_output = _pretty_print_output;
  };
};

void
makeStructure(std::string absfilepath,
              std::string filename,
              std::ostream * outFile,
              std::ostream * gapsFile,
              std::string gaps_filenm,
              std::string search_path,
              Struct::Options & opts);

} // namespace Struct
} // namespace BAnal

#endif // BAnal_Struct_hpp
