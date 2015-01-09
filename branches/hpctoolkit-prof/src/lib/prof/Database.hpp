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
// Copyright ((c)) 2002-2015, Rice University
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

#ifndef Prof_Database_hpp
#define Prof_Database_hpp

#include <sys/types.h>
#include <stdint.h>

#include <include/uint.h>
#include <lib/analysis/Args.hpp>
#include <lib/analysis/ArgsHPCProf.hpp>
#include <lib/prof/CCT-Merge.hpp>
#include <lib/prof/CallPath-Profile.hpp>

namespace Prof {
namespace Database {

struct traceInfo_s {
  uint64_t  min_time;
  uint64_t  max_time;
  off_t  start_offset;
  off_t  length;
  long   rank;
  long   tid;
  bool   active;
};

typedef struct traceInfo_s traceInfo;

void initdb(const Analysis::Args & args);

bool newDBFormat(void);

off_t alignOffset(off_t offset);

bool makeSummaryDB(Prof::CallPath::Profile & prof,
		   const Analysis::Args & args);

off_t firstTraceOffset(long num_files);

int writeTraceHeader(traceInfo *trace, long num_threads, long num_active);

int writeTraceIndex(traceInfo *trace, long num_threads, long num_active);

int writeTraceFile(Prof::CallPath::Profile *prof, 
		   Prof::CCT::MergeEffectList *effects);

void endTraceFiles(void);

int writeThreadIDFile(traceInfo *trace, long num_threads);

int writePlotHeader(int fd, ulong max_cctid, ulong num_metrics, ulong num_threads,
		    ulong index_start, ulong index_length,
		    ulong plot_start, ulong plot_length);

}  // namespace Prof, Database
}

#endif
