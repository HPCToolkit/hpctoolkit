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

#include "sparse.hpp"
#include <sys/types.h>
#include <sys/stat.h>

#include<cmath>
#include <omp.h>

#include <sstream>

using namespace hpctoolkit;

// Merge function, only called on rank 0. Split since there's the extra
// argument with all the bits.
void SparseDB::merge0(int threads, MPI_File& outfile, const std::vector<std::pair<ThreadAttributes,
    stdshim::filesystem::path>>& inputs) {
      
  // At the moment, all we do is write out a series of lines about what files
  // would have been read in and such.
  // Since MPI makes this complicated, we stash in a stringstream and then WRITE.
  std::ostringstream ss;
  for(const auto& in: inputs) {
    const auto& attr = in.first;
    //ss << "Thread [" << attr.mpirank() << "," << attr.threadid() << "] "
    //   << "written in " << in.second.string() << "\n";

    //YUMENG
    struct stat buf;
    stat(in.second.string().c_str(),&buf);
    ss << "Thread [" << attr.mpirank() << "," << attr.threadid() << "] "
       << "written in " << in.second.string() << "with size: " << buf.st_size <<"\n";
  }
  std::string s = ss.str();

  // Do the actual write
  MPI_Status stat;
  MPI_File_write(outfile, s.c_str(), s.size(), MPI_CHAR, &stat);
}

// Merge function, only called on rank >0.
void SparseDB::mergeN(int threads, MPI_File& outfile) {
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  std::vector<std::pair<ThreadAttributes, stdshim::filesystem::path>> my_inputs;
  
}
