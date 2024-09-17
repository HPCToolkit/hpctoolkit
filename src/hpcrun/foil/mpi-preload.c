// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "common-preload.h"
#include "common.h"
#include "mpi-private.h"

#include <dlfcn.h>
#include <threads.h>

static struct hpcrun_foil_appdispatch_mpi dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_mpi){
      .MPI_Comm_rank = foil_dlsym("MPI_Comm_rank"),
      .mpi_comm_rank = foil_dlsym("mpi_comm_rank"),
      .mpi_comm_rank_ = foil_dlsym("mpi_comm_rank_"),
      .mpi_comm_rank__ = foil_dlsym("mpi_comm_rank__"),
      .MPI_Comm_size = foil_dlsym("MPI_Comm_size"),
      .mpi_comm_size = foil_dlsym("mpi_comm_size"),
      .mpi_comm_size_ = foil_dlsym("mpi_comm_size_"),
      .mpi_comm_size__ = foil_dlsym("mpi_comm_size__"),
      .MPI_Finalize = foil_dlsym("MPI_Finalize"),
      .mpi_finalize = foil_dlsym("mpi_finalize"),
      .mpi_finalize_ = foil_dlsym("mpi_finalize_"),
      .mpi_finalize__ = foil_dlsym("mpi_finalize__"),
      .MPI_Init = foil_dlsym("MPI_Init"),
      .mpi_init = foil_dlsym("mpi_init"),
      .mpi_init_ = foil_dlsym("mpi_init_"),
      .mpi_init__ = foil_dlsym("mpi_init__"),
      .MPI_Init_thread = foil_dlsym("MPI_Init_thread"),
      .mpi_init_thread = foil_dlsym("mpi_init_thread"),
      .mpi_init_thread_ = foil_dlsym("mpi_init_thread_"),
      .mpi_init_thread__ = foil_dlsym("mpi_init_thread__"),
      .PMPI_Init = foil_dlsym("PMPI_Init"),
      .pmpi_init = foil_dlsym("pmpi_init"),
      .pmpi_init_ = foil_dlsym("pmpi_init_"),
      .pmpi_init__ = foil_dlsym("pmpi_init__"),
      .PMPI_Init_thread = foil_dlsym("PMPI_Init_thread"),
      .pmpi_init_thread = foil_dlsym("pmpi_init_thread"),
      .pmpi_init_thread_ = foil_dlsym("pmpi_init_thread_"),
      .pmpi_init_thread__ = foil_dlsym("pmpi_init_thread__"),
      .PMPI_Finalize = foil_dlsym("PMPI_Finalize"),
      .pmpi_finalize = foil_dlsym("pmpi_finalize"),
      .pmpi_finalize_ = foil_dlsym("pmpi_finalize_"),
      .pmpi_finalize__ = foil_dlsym("pmpi_finalize__"),
      .PMPI_Comm_rank = foil_dlsym("PMPI_Comm_rank"),
      .pmpi_comm_rank = foil_dlsym("pmpi_comm_rank"),
      .pmpi_comm_rank_ = foil_dlsym("pmpi_comm_rank_"),
      .pmpi_comm_rank__ = foil_dlsym("pmpi_comm_rank__"),
      .PMPI_Comm_size = foil_dlsym("PMPI_Comm_size"),
      .pmpi_comm_size = foil_dlsym("pmpi_comm_size"),
      .pmpi_comm_size_ = foil_dlsym("pmpi_comm_size_"),
      .pmpi_comm_size__ = foil_dlsym("pmpi_comm_size__"),
  };
}

static const struct hpcrun_foil_appdispatch_mpi* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API int MPI_Comm_rank(void* comm, int* rank) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->MPI_Comm_rank(comm, rank, dispatch());
}

HPCRUN_EXPOSED_API void mpi_comm_rank(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_comm_rank(comm, rank, ierror,
                                                         dispatch());
}

HPCRUN_EXPOSED_API void mpi_comm_rank_(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_comm_rank_(comm, rank, ierror,
                                                          dispatch());
}

HPCRUN_EXPOSED_API void mpi_comm_rank__(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_comm_rank__(comm, rank, ierror,
                                                           dispatch());
}

HPCRUN_EXPOSED_API int MPI_Finalize() {
  return hpcrun_foil_fetch_hooks_mpi_dl()->MPI_Finalize(dispatch());
}

HPCRUN_EXPOSED_API void mpi_finalize(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_finalize(ierror, dispatch());
}

HPCRUN_EXPOSED_API void mpi_finalize_(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_finalize_(ierror, dispatch());
}

HPCRUN_EXPOSED_API void mpi_finalize__(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_finalize__(ierror, dispatch());
}

HPCRUN_EXPOSED_API int MPI_Init(int* argc, char*** argv) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->MPI_Init(argc, argv, dispatch());
}

HPCRUN_EXPOSED_API void mpi_init(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init(ierror, dispatch());
}

HPCRUN_EXPOSED_API void mpi_init_(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init_(ierror, dispatch());
}

HPCRUN_EXPOSED_API void mpi_init__(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init__(ierror, dispatch());
}

HPCRUN_EXPOSED_API int MPI_Init_thread(int* argc, char*** argv, int required,
                                       int* provided) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->MPI_Init_thread(argc, argv, required,
                                                           provided, dispatch());
}

HPCRUN_EXPOSED_API void mpi_init_thread(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init_thread(required, provided, ierror,
                                                           dispatch());
}

HPCRUN_EXPOSED_API void mpi_init_thread_(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init_thread_(required, provided, ierror,
                                                            dispatch());
}

HPCRUN_EXPOSED_API void mpi_init_thread__(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->mpi_init_thread__(required, provided, ierror,
                                                             dispatch());
}

HPCRUN_EXPOSED_API int PMPI_Init(int* argc, char*** argv) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->PMPI_Init(argc, argv, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init(ierror, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init_(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init_(ierror, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init__(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init__(ierror, dispatch());
}

HPCRUN_EXPOSED_API int PMPI_Init_thread(int* argc, char*** argv, int required,
                                        int* provided) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->PMPI_Init_thread(argc, argv, required,
                                                            provided, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init_thread(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init_thread(required, provided, ierror,
                                                            dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init_thread_(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init_thread_(required, provided, ierror,
                                                             dispatch());
}

HPCRUN_EXPOSED_API void pmpi_init_thread__(int* required, int* provided, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_init_thread__(required, provided,
                                                              ierror, dispatch());
}

HPCRUN_EXPOSED_API int PMPI_Finalize(void) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->PMPI_Finalize(dispatch());
}

HPCRUN_EXPOSED_API void pmpi_finalize(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_finalize(ierror, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_finalize_(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_finalize_(ierror, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_finalize__(int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_finalize__(ierror, dispatch());
}

HPCRUN_EXPOSED_API int PMPI_Comm_rank(void* comm, int* rank) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->PMPI_Comm_rank(comm, rank, dispatch());
}

HPCRUN_EXPOSED_API void pmpi_comm_rank(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_comm_rank(comm, rank, ierror,
                                                          dispatch());
}

HPCRUN_EXPOSED_API void pmpi_comm_rank_(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_comm_rank_(comm, rank, ierror,
                                                           dispatch());
}

HPCRUN_EXPOSED_API void pmpi_comm_rank__(int* comm, int* rank, int* ierror) {
  return hpcrun_foil_fetch_hooks_mpi_dl()->pmpi_comm_rank__(comm, rank, ierror,
                                                            dispatch());
}
