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
// Copyright ((c)) 2002-2023, Rice University
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

#define _GNU_SOURCE

#include "foil.h"
#include "../libmonitor/monitor.h"

HPCRUN_EXPOSED int MPI_Comm_rank(void *comm, int *rank) {
  LOOKUP_FOIL_BASE(base, MPI_Comm_rank);
  return base(comm, rank);
}

HPCRUN_EXPOSED void mpi_comm_rank(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_comm_rank);
  return base(comm, rank, ierror);
}

HPCRUN_EXPOSED void mpi_comm_rank_(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_comm_rank_);
  return base(comm, rank, ierror);
}

HPCRUN_EXPOSED void mpi_comm_rank__(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_comm_rank__);
  return base(comm, rank, ierror);
}

HPCRUN_EXPOSED int MPI_Finalize() {
  LOOKUP_FOIL_BASE(base, MPI_Finalize);
  return base();
}

HPCRUN_EXPOSED void mpi_finalize(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_finalize);
  return base(ierror);
}

HPCRUN_EXPOSED void mpi_finalize_(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_finalize_);
  return base(ierror);
}

HPCRUN_EXPOSED void mpi_finalize__(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_finalize__);
  return base(ierror);
}

HPCRUN_EXPOSED int MPI_Init(int *argc, char ***argv) {
  LOOKUP_FOIL_BASE(base, MPI_Init);
  return base(argc, argv);
}

HPCRUN_EXPOSED void mpi_init(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init);
  return base(ierror);
}

HPCRUN_EXPOSED void mpi_init_(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init_);
  return base(ierror);
}

HPCRUN_EXPOSED void mpi_init__(int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init__);
  return base(ierror);
}

HPCRUN_EXPOSED int MPI_Init_thread(int *argc, char ***argv, int required, int *provided) {
  LOOKUP_FOIL_BASE(base, MPI_Init_thread);
  return base(argc, argv, required, provided);
}

HPCRUN_EXPOSED void mpi_init_thread(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init_thread);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED void mpi_init_thread_(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init_thread_);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED void mpi_init_thread__(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, mpi_init_thread__);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED int PMPI_Init(int *argc, char ***argv) {
  LOOKUP_FOIL_BASE(base, PMPI_Init);
  return base(argc, argv);
}

HPCRUN_EXPOSED void pmpi_init(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init);
  return base(ierror);
}

HPCRUN_EXPOSED void pmpi_init_(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init_);
  return base(ierror);
}

HPCRUN_EXPOSED void pmpi_init__(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init__);
  return base(ierror);
}

HPCRUN_EXPOSED int PMPI_Init_thread(int *argc, char ***argv, int required, int *provided) {
  LOOKUP_FOIL_BASE(base, PMPI_Init_thread);
  return base(argc, argv, required, provided);
}

HPCRUN_EXPOSED void pmpi_init_thread(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init_thread);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED void pmpi_init_thread_(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init_thread_);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED void pmpi_init_thread__(int *required, int *provided, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_init_thread__);
  return base(required, provided, ierror);
}

HPCRUN_EXPOSED int PMPI_Finalize(void) {
  LOOKUP_FOIL_BASE(base, PMPI_Finalize);
  return base();
}

HPCRUN_EXPOSED void pmpi_finalize(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_finalize);
  return base(ierror);
}

HPCRUN_EXPOSED void pmpi_finalize_(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_finalize_);
  return base(ierror);
}

HPCRUN_EXPOSED void pmpi_finalize__(int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_finalize__);
  return base(ierror);
}

HPCRUN_EXPOSED int PMPI_Comm_rank(void *comm, int *rank) {
  LOOKUP_FOIL_BASE(base, PMPI_Comm_rank);
  return base(comm, rank);
}

HPCRUN_EXPOSED void pmpi_comm_rank(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_comm_rank);
  return base(comm, rank, ierror);
}

HPCRUN_EXPOSED void pmpi_comm_rank_(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_comm_rank_);
  return base(comm, rank, ierror);
}

HPCRUN_EXPOSED void pmpi_comm_rank__(int *comm, int *rank, int *ierror) {
  LOOKUP_FOIL_BASE(base, pmpi_comm_rank__);
  return base(comm, rank, ierror);
}
