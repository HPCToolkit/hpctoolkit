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

#include "../hpcprof2/args.hpp"

#include <mpi.h>
#include <iostream>

std::mutex mpitex;

using namespace hpctoolkit;

// The code for rank 0 and all other ranks are separated due to their
// overall difference.
int rank0(ProfArgs&&, int, int);
int rankN(ProfArgs&&, int, int);

static bool done = false;
void escape() {
  // We use std::exit when things go south, but MPI doesn't react very quickly
  // to that. So we issue an Abort ourselves.
  if(!done) MPI_Abort(MPI_COMM_WORLD, 0);
}

int main(int argc, char* const argv[]) {
  // Read in the arguments.
  ProfArgs args(argc, argv);

  // Fire up MPI. We just use the WORLD communicator for everything.
  int available;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &available);
  std::atexit(escape);
  if(available < MPI_THREAD_MULTIPLE)
    util::log::fatal{} << "MPI does not have full thread support!";
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Because rank 0 is acts so differently from the others, its easier to
  // split the program into two copies that handle their own bits.
  // It also helps ensure MPI_Finalize is called at the end like it should.
  // Any complex common bits are factored out into individual functions.
  int ret;
  if(world_rank == 0)
    ret = rank0(std::move(args), world_rank, world_size);
  else
    ret = rankN(std::move(args), world_rank, world_size);

  // Clean up and close up.
  MPI_Finalize();
  done = true;
  return ret;
}
