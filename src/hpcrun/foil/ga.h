// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_GA_H
#define HPCRUN_FOIL_GA_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hpcrun_foil_appdispatch_ga;

// FIXME: temporarily import GA 5.2 declarations.  Unfortunately, some
// of these declarations are only in the source tree (as opposed to
// the installation), so currently there is no clean solution.

// Need: field for a profiler

// ${GA-install}/include/typesf2c.h
// ${GA-build}/gaf2c/typesf2c.h
// ${GA-src}/gaf2c/typesf2c.h.in [F2C_INTEGER_C_TYPE]
typedef int Integer;
typedef Integer logical;

// ${GA-install}/include/armci.h
// ${GA-src}/armci/src/include/armci.h
typedef long armci_size_t;

// ${GA-install}/include/gacommon.h
// ${GA-src}/global/src/gacommon.h
#define GA_MAX_DIM 7

// ${GA-src}/global/src/gaconfig.h
#define MAXDIM GA_MAX_DIM

// ${GA-src}/global/src/globalp.h
#define GA_OFFSET 1000

// ${GA-src}/global/src/base.h
#define FNAM 31

typedef Integer C_Integer;
typedef armci_size_t C_Long;

typedef struct {
  short int ndim;               ///< number of dimensions
  short int irreg;              ///< 0-regular; 1-irregular distribution
  int type;                     ///< data type in array
  int actv;                     ///< activity status, GA is allocated
  int actv_handle;              ///< handle is created
  C_Long size;                  ///< size of local data in bytes
  int elemsize;                 ///< sizeof(datatype)
  int ghosts;                   ///< flag indicating presence of ghosts
  long lock;                    ///< lock
  long id;                      ///< ID of shmem region / MA handle
  C_Integer dims[MAXDIM];       ///< global array dimensions
  C_Integer chunk[MAXDIM];      ///< chunking
  int nblock[MAXDIM];           ///< number of blocks per dimension
  C_Integer width[MAXDIM];      ///< boundary cells per dimension
  C_Integer first[MAXDIM];      ///< (Mirrored only) first local element
  C_Integer last[MAXDIM];       ///< (Mirrored only) last local element
  C_Long shm_length;            ///< (Mirrored only) local shmem length
  C_Integer lo[MAXDIM];         ///< top/left corner in local patch
  double scale[MAXDIM];         ///< nblock/dim (precomputed)
  char** ptr;                   ///< arrays of pointers to remote data
  C_Integer* mapc;              ///< lock distribution map
  char name[FNAM + 1];          ///< array name
  int p_handle;                 ///< pointer to processor list for array
  double* cache;                ///< store for frequently accessed ptrs
  int corner_flag;              ///< flag for updating corner ghost cells
  int block_flag;               ///< flag to indicate block-cyclic data
  int block_sl_flag;            ///< flag to indicate block-cyclic data
                                /// using ScaLAPACK format
  C_Integer block_dims[MAXDIM]; ///< array of block dimensions
  C_Integer num_blocks[MAXDIM]; ///< number of blocks in each dimension
  C_Integer block_total;        ///< total number of blocks in array
                                /// using restricted arrays
  C_Integer* rstrctd_list;      ///< list of processors with data
  C_Integer num_rstrctd;        ///< number of processors with data
  C_Integer has_data;           ///< flag that processor has data
  C_Integer rstrctd_id;         ///< rank of processor in restricted list
  C_Integer* rank_rstrctd;      ///< ranks of processors with data
} global_array_t;

global_array_t* f_GA(const struct hpcrun_foil_appdispatch_ga*);
logical f_pnga_create(Integer type, Integer ndim, Integer* dims, char* name,
                      Integer* chunk, Integer* g_a,
                      const struct hpcrun_foil_appdispatch_ga*);
Integer f_pnga_create_handle(const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_get(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_put(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_acc(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                void* alpha, const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_nbget(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_nbput(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_nbacc(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld,
                  void* alpha, Integer* nbhandle,
                  const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_nbwait(Integer* nbhandle, const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_brdcst(Integer type, void* buf, Integer len, Integer originator,
                   const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_gop(Integer type, void* x, Integer n, char* op,
                const struct hpcrun_foil_appdispatch_ga*);
void f_pnga_sync(const struct hpcrun_foil_appdispatch_ga*);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_GA_PRIVATE_H
