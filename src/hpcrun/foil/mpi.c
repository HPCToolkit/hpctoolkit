// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "mpi.h"

#include "../libmonitor/monitor.h"
#include "common.h"
#include "mpi-private.h"

const struct hpcrun_foil_hookdispatch_mpi* hpcrun_foil_fetch_hooks_mpi() {
  static const struct hpcrun_foil_hookdispatch_mpi hooks = {
      .MPI_Comm_rank = hpcrun_MPI_Comm_rank,
      .mpi_comm_rank = hpcrun_mpi_comm_rank_fortran0,
      .mpi_comm_rank_ = hpcrun_mpi_comm_rank_fortran1,
      .mpi_comm_rank__ = hpcrun_mpi_comm_rank_fortran2,

      .MPI_Finalize = hpcrun_MPI_Finalize,
      .mpi_finalize = hpcrun_mpi_finalize_fortran0,
      .mpi_finalize_ = hpcrun_mpi_finalize_fortran1,
      .mpi_finalize__ = hpcrun_mpi_finalize_fortran2,

      .MPI_Init = hpcrun_MPI_Init,
      .mpi_init = hpcrun_mpi_init_fortran0,
      .mpi_init_ = hpcrun_mpi_init_fortran1,
      .mpi_init__ = hpcrun_mpi_init_fortran2,

      .MPI_Init_thread = hpcrun_MPI_Init_thread,
      .mpi_init_thread = hpcrun_mpi_init_thread_fortran0,
      .mpi_init_thread_ = hpcrun_mpi_init_thread_fortran1,
      .mpi_init_thread__ = hpcrun_mpi_init_thread_fortran2,

      .PMPI_Init = hpcrun_PMPI_Init,
      .pmpi_init = hpcrun_pmpi_init_fortran0,
      .pmpi_init_ = hpcrun_pmpi_init_fortran1,
      .pmpi_init__ = hpcrun_pmpi_init_fortran2,

      .PMPI_Init_thread = hpcrun_PMPI_Init_thread,
      .pmpi_init_thread = hpcrun_pmpi_init_thread_fortran0,
      .pmpi_init_thread_ = hpcrun_pmpi_init_thread_fortran1,
      .pmpi_init_thread__ = hpcrun_pmpi_init_thread_fortran2,

      .PMPI_Finalize = hpcrun_PMPI_Finalize,
      .pmpi_finalize = hpcrun_pmpi_finalize_fortran0,
      .pmpi_finalize_ = hpcrun_pmpi_finalize_fortran1,
      .pmpi_finalize__ = hpcrun_pmpi_finalize_fortran2,

      .PMPI_Comm_rank = hpcrun_PMPI_Comm_rank,
      .pmpi_comm_rank = hpcrun_pmpi_comm_rank_fortran0,
      .pmpi_comm_rank_ = hpcrun_pmpi_comm_rank_fortran1,
      .pmpi_comm_rank__ = hpcrun_pmpi_comm_rank_fortran2,
  };
  return &hooks;
}

int f_MPI_Comm_rank(void* comm, int* rank,
                    const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->MPI_Comm_rank(comm, rank);
}

void f_mpi_comm_rank_fortran0(void* comm, int* rank, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_rank(comm, rank, ierror);
}

void f_mpi_comm_rank_fortran1(void* comm, int* rank, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_rank_(comm, rank, ierror);
}

void f_mpi_comm_rank_fortran2(void* comm, int* rank, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_rank__(comm, rank, ierror);
}

int f_MPI_Comm_size(void* comm, int* size,
                    const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->MPI_Comm_size(comm, size);
}

void f_mpi_comm_size_fortran0(void* comm, int* size, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_size(comm, size, ierror);
}

void f_mpi_comm_size_fortran1(void* comm, int* size, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_size_(comm, size, ierror);
}

void f_mpi_comm_size_fortran2(void* comm, int* size, int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_comm_size__(comm, size, ierror);
}

int f_MPI_Finalize(const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->MPI_Finalize();
}

void f_mpi_finalize_fortran0(int* ierror,
                             const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_finalize(ierror);
}

void f_mpi_finalize_fortran1(int* ierror,
                             const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_finalize_(ierror);
}

void f_mpi_finalize_fortran2(int* ierror,
                             const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_finalize__(ierror);
}

int f_MPI_Init(int* argc, char*** argv,
               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->MPI_Init(argc, argv);
}

void f_mpi_init_fortran0(int* ierror,
                         const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init(ierror);
}

void f_mpi_init_fortran1(int* ierror,
                         const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init_(ierror);
}

void f_mpi_init_fortran2(int* ierror,
                         const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init__(ierror);
}

int f_MPI_Init_thread(int* argc, char*** argv, int required, int* provided,
                      const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->MPI_Init_thread(argc, argv, required, provided);
}

void f_mpi_init_thread_fortran0(int* required, int* provided, int* ierror,
                                const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init_thread(required, provided, ierror);
}

void f_mpi_init_thread_fortran1(int* required, int* provided, int* ierror,
                                const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init_thread_(required, provided, ierror);
}

void f_mpi_init_thread_fortran2(int* required, int* provided, int* ierror,
                                const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->mpi_init_thread__(required, provided, ierror);
}

int f_PMPI_Init(int* argc, char*** argv,
                const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->PMPI_Init(argc, argv);
}

void f_pmpi_init_fortran0(int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init(ierror);
}

void f_pmpi_init_fortran1(int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init_(ierror);
}

void f_pmpi_init_fortran2(int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init__(ierror);
}

int f_PMPI_Init_thread(int* argc, char*** argv, int required, int* provided,
                       const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->PMPI_Init_thread(argc, argv, required, provided);
}

void f_pmpi_init_thread_fortran0(int* required, int* provided, int* ierror,
                                 const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init_thread(required, provided, ierror);
}

void f_pmpi_init_thread_fortran1(int* required, int* provided, int* ierror,
                                 const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init_thread_(required, provided, ierror);
}

void f_pmpi_init_thread_fortran2(int* required, int* provided, int* ierror,
                                 const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_init_thread__(required, provided, ierror);
}

int f_PMPI_Finalize(const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->PMPI_Finalize();
}

void f_pmpi_finalize_fortran0(int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_finalize(ierror);
}

void f_pmpi_finalize_fortran1(int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_finalize_(ierror);
}

void f_pmpi_finalize_fortran2(int* ierror,
                              const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_finalize__(ierror);
}

int f_PMPI_Comm_rank(void* comm, int* rank,
                     const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->PMPI_Comm_rank(comm, rank);
}

void f_pmpi_comm_rank_fortran0(void* comm, int* rank, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_rank(comm, rank, ierror);
}

void f_pmpi_comm_rank_fortran1(void* comm, int* rank, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_rank_(comm, rank, ierror);
}

void f_pmpi_comm_rank_fortran2(void* comm, int* rank, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_rank__(comm, rank, ierror);
}

int f_PMPI_Comm_size(void* comm, int* size,
                     const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->PMPI_Comm_size(comm, size);
}

void f_pmpi_comm_size_fortran0(void* comm, int* size, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_size(comm, size, ierror);
}

void f_pmpi_comm_size_fortran1(void* comm, int* size, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_size_(comm, size, ierror);
}

void f_pmpi_comm_size_fortran2(void* comm, int* size, int* ierror,
                               const struct hpcrun_foil_appdispatch_mpi* dispatch) {
  return dispatch->pmpi_comm_size__(comm, size, ierror);
}
