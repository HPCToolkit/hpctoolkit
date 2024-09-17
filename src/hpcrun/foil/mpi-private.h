// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_MPI_PRIVATE_H
#define HPCRUN_FOIL_MPI_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

struct hpcrun_foil_appdispatch_mpi {
  int (*MPI_Comm_rank)(void* comm, int* rank);
  void (*mpi_comm_rank)(int* comm, int* rank, int* ierror);
  void (*mpi_comm_rank_)(int* comm, int* rank, int* ierror);
  void (*mpi_comm_rank__)(int* comm, int* rank, int* ierror);

  int (*MPI_Comm_size)(void* comm, int* size);
  void (*mpi_comm_size)(int* comm, int* size, int* ierror);
  void (*mpi_comm_size_)(int* comm, int* size, int* ierror);
  void (*mpi_comm_size__)(int* comm, int* size, int* ierror);

  int (*MPI_Finalize)();
  void (*mpi_finalize)(int* ierror);
  void (*mpi_finalize_)(int* ierror);
  void (*mpi_finalize__)(int* ierror);

  int (*MPI_Init)(int* argc, char*** argv);
  void (*mpi_init)(int* ierror);
  void (*mpi_init_)(int* ierror);
  void (*mpi_init__)(int* ierror);

  int (*MPI_Init_thread)(int* argc, char*** argv, int required, int* provided);
  void (*mpi_init_thread)(int* required, int* provided, int* ierror);
  void (*mpi_init_thread_)(int* required, int* provided, int* ierror);
  void (*mpi_init_thread__)(int* required, int* provided, int* ierror);

  int (*PMPI_Init)(int* argc, char*** argv);
  void (*pmpi_init)(int* ierror);
  void (*pmpi_init_)(int* ierror);
  void (*pmpi_init__)(int* ierror);

  int (*PMPI_Init_thread)(int* argc, char*** argv, int required, int* provided);
  void (*pmpi_init_thread)(int* required, int* provided, int* ierror);
  void (*pmpi_init_thread_)(int* required, int* provided, int* ierror);
  void (*pmpi_init_thread__)(int* required, int* provided, int* ierror);

  int (*PMPI_Finalize)();
  void (*pmpi_finalize)(int* ierror);
  void (*pmpi_finalize_)(int* ierror);
  void (*pmpi_finalize__)(int* ierror);

  int (*PMPI_Comm_rank)(void* comm, int* rank);
  void (*pmpi_comm_rank)(int* comm, int* rank, int* ierror);
  void (*pmpi_comm_rank_)(int* comm, int* rank, int* ierror);
  void (*pmpi_comm_rank__)(int* comm, int* rank, int* ierror);

  int (*PMPI_Comm_size)(void* comm, int* size);
  void (*pmpi_comm_size)(int* comm, int* size, int* ierror);
  void (*pmpi_comm_size_)(int* comm, int* size, int* ierror);
  void (*pmpi_comm_size__)(int* comm, int* size, int* ierror);
};

struct hpcrun_foil_hookdispatch_mpi {
  int (*MPI_Comm_rank)(void* comm, int* rank,
                       const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_comm_rank)(int* comm, int* rank, int* ierror,
                        const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_comm_rank_)(int* comm, int* rank, int* ierror,
                         const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_comm_rank__)(int* comm, int* rank, int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi*);

  int (*MPI_Finalize)(const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_finalize)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_finalize_)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_finalize__)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);

  int (*MPI_Init)(int* argc, char*** argv, const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init_)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init__)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);

  int (*MPI_Init_thread)(int* argc, char*** argv, int required, int* provided,
                         const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init_thread)(int* required, int* provided, int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init_thread_)(int* required, int* provided, int* ierror,
                           const struct hpcrun_foil_appdispatch_mpi*);
  void (*mpi_init_thread__)(int* required, int* provided, int* ierror,
                            const struct hpcrun_foil_appdispatch_mpi*);

  int (*PMPI_Init)(int* argc, char*** argv, const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init_)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init__)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);

  int (*PMPI_Init_thread)(int* argc, char*** argv, int required, int* provided,
                          const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init_thread)(int* required, int* provided, int* ierror,
                           const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init_thread_)(int* required, int* provided, int* ierror,
                            const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_init_thread__)(int* required, int* provided, int* ierror,
                             const struct hpcrun_foil_appdispatch_mpi*);

  int (*PMPI_Finalize)(const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_finalize)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_finalize_)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_finalize__)(int* ierror, const struct hpcrun_foil_appdispatch_mpi*);

  int (*PMPI_Comm_rank)(void* comm, int* rank,
                        const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_comm_rank)(int* comm, int* rank, int* ierror,
                         const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_comm_rank_)(int* comm, int* rank, int* ierror,
                          const struct hpcrun_foil_appdispatch_mpi*);
  void (*pmpi_comm_rank__)(int* comm, int* rank, int* ierror,
                           const struct hpcrun_foil_appdispatch_mpi*);
};

#endif // HPCRUN_FOIL_MPI_PRIVATE_H
