// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_MPI_H
#define HPCRUN_FOIL_MPI_H

#ifdef __cplusplus
extern "C" {
#endif

struct hpcrun_foil_appdispatch_mpi;

int f_MPI_Init(int*, char***, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_fortran0(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_fortran1(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_fortran2(int*, const struct hpcrun_foil_appdispatch_mpi*);

int f_MPI_Init_thread(int*, char***, int, int*,
                      const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_thread_fortran0(int*, int*, int*,
                                const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_thread_fortran1(int*, int*, int*,
                                const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_init_thread_fortran2(int*, int*, int*,
                                const struct hpcrun_foil_appdispatch_mpi*);

int f_MPI_Finalize(const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_finalize_fortran0(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_finalize_fortran1(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_finalize_fortran2(int*, const struct hpcrun_foil_appdispatch_mpi*);

int f_MPI_Comm_size(void*, int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_size_fortran0(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_size_fortran1(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_size_fortran2(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);

int f_MPI_Comm_rank(void*, int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_rank_fortran0(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_rank_fortran1(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);
void f_mpi_comm_rank_fortran2(void*, int*, int*,
                              const struct hpcrun_foil_appdispatch_mpi*);

int f_PMPI_Init(int*, char***, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_fortran0(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_fortran1(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_fortran2(int*, const struct hpcrun_foil_appdispatch_mpi*);

int f_PMPI_Init_thread(int*, char***, int, int*,
                       const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_thread_fortran0(int*, int*, int*,
                                 const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_thread_fortran1(int*, int*, int*,
                                 const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_init_thread_fortran2(int*, int*, int*,
                                 const struct hpcrun_foil_appdispatch_mpi*);

int f_PMPI_Finalize(const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_finalize_fortran0(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_finalize_fortran1(int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_finalize_fortran2(int*, const struct hpcrun_foil_appdispatch_mpi*);

int f_PMPI_Comm_size(void*, int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_size_fortran0(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_size_fortran1(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_size_fortran2(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);

int f_PMPI_Comm_rank(void*, int*, const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_rank_fortran0(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_rank_fortran1(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);
void f_pmpi_comm_rank_fortran2(void*, int*, int*,
                               const struct hpcrun_foil_appdispatch_mpi*);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_MPI_H
