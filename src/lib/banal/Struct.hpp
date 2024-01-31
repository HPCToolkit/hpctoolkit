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
// Copyright ((c)) 2002-2024, Rice University
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
makeStructure(std::string filename,
              std::ostream * outFile,
              std::ostream * gapsFile,
              std::string gaps_filenm,
              std::string search_path,
              Struct::Options & opts);

} // namespace Struct
} // namespace BAnal

#endif // BAnal_Struct_hpp
