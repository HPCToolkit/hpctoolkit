// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HPCRUN_SS_GA_OVERRIDES_H
#define HPCRUN_SS_GA_OVERRIDES_H

// FIXME: temporarily import GA 5.2 declarations.  Unfortunately, some
// of these declarations are only in the source tree (as opposed to
// the installation), so currently there is no clean solution.

// Need: field for a profiler

// ${GA-install}/include/typesf2c.h
// ${GA-build}/gaf2c/typesf2c.h
// ${GA-src}/gaf2c/typesf2c.h.in [F2C_INTEGER_C_TYPE]
typedef int Integer; // integer size
typedef Integer logical;

// ${GA-install}/include/armci.h
// ${GA-src}/armci/src/include/armci.h
typedef long armci_size_t;

// ${GA-install}/include/gacommon.h
// ${GA-src}/global/src/gacommon.h
#define GA_MAX_DIM 7

// ${GA-src}/global/src/gaconfig.h
#define MAXDIM  GA_MAX_DIM

// ${GA-src}/global/src/globalp.h
#define GA_OFFSET   1000

// ${GA-src}/global/src/base.h
#define FNAM        31              /* length of array names   */

typedef Integer C_Integer;
typedef armci_size_t C_Long;

typedef struct {
       short int  ndim;             /* number of dimensions                 */
       short int  irreg;            /* 0-regular; 1-irregular distribution  */
       int  type;                   /* data type in array                   */
       int  actv;                   /* activity status, GA is allocated     */
       int  actv_handle;            /* handle is created                    */
       C_Long   size;               /* size of local data in bytes          */
       int  elemsize;               /* sizeof(datatype)                     */
       int  ghosts;                 /* flag indicating presence of ghosts   */
       long lock;                   /* lock                                 */
       long id;                     /* ID of shmem region / MA handle       */
       C_Integer  dims[MAXDIM];     /* global array dimensions              */
       C_Integer  chunk[MAXDIM];    /* chunking                             */
       int  nblock[MAXDIM];         /* number of blocks per dimension       */
       C_Integer  width[MAXDIM];    /* boundary cells per dimension         */
       C_Integer  first[MAXDIM];    /* (Mirrored only) first local element  */
       C_Integer  last[MAXDIM];     /* (Mirrored only) last local element   */
       C_Long  shm_length;          /* (Mirrored only) local shmem length   */
       C_Integer  lo[MAXDIM];       /* top/left corner in local patch       */
       double scale[MAXDIM];        /* nblock/dim (precomputed)             */
       char **ptr;                  /* arrays of pointers to remote data    */
       C_Integer  *mapc;            /* block distribution map               */
       char name[FNAM+1];           /* array name                           */
       int p_handle;                /* pointer to processor list for array  */
       double *cache;               /* store for frequently accessed ptrs   */
       int corner_flag;             /* flag for updating corner ghost cells */
       int block_flag;              /* flag to indicate block-cyclic data   */
       int block_sl_flag;           /* flag to indicate block-cyclic data   */
                                    /* using ScaLAPACK format               */
       C_Integer block_dims[MAXDIM];/* array of block dimensions            */
       C_Integer num_blocks[MAXDIM];/* number of blocks in each dimension   */
       C_Integer block_total;       /* total number of blocks in array      */
                                    /* using restricted arrays              */
       C_Integer *rstrctd_list;     /* list of processors with data         */
       C_Integer num_rstrctd;       /* number of processors with data       */
       C_Integer has_data;          /* flag that processor has data         */
       C_Integer rstrctd_id;        /* rank of processor in restricted list */
       C_Integer *rank_rstrctd;     /* ranks of processors with data        */

#ifdef ENABLE_CHECKPOINT
       int record_id;               /* record id for writing ga to disk     */
#endif
} global_array_t;

typedef logical ga_create_fn_t(Integer type, Integer ndim,
                               Integer *dims, char* name,
                               Integer *chunk, Integer *g_a);
extern ga_create_fn_t pnga_create;
logical
foilbase_pnga_create(ga_create_fn_t* real_pnga_create, global_array_t* GA,
    Integer type, Integer ndim, Integer *dims, char* name,
    Integer *chunk, Integer *g_a);

typedef Integer ga_create_handle_fn_t();
extern ga_create_handle_fn_t pnga_create_handle;
Integer
foilbase_pnga_create_handle(ga_create_handle_fn_t* real_pnga_create_handle, global_array_t* GA);

typedef void ga_getput_fn_t(Integer g_a, Integer* lo, Integer* hi, void* buf, Integer* ld);

extern ga_getput_fn_t pnga_get;
void
foilbase_pnga_get(ga_getput_fn_t* real_pnga_get, global_array_t* GA, Integer g_a, Integer* lo,
                  Integer* hi, void* buf, Integer* ld);

extern ga_getput_fn_t pnga_put;
void
foilbase_pnga_put(ga_getput_fn_t* real_pnga_put, global_array_t* GA, Integer g_a, Integer* lo,
                  Integer* hi, void* buf, Integer* ld);

typedef void ga_acc_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha);
extern ga_acc_fn_t pnga_acc;
void
foilbase_pnga_acc(ga_acc_fn_t* real_pnga_acc, global_array_t* GA, Integer g_a, Integer *lo,
                  Integer *hi, void *buf, Integer *ld, void *alpha);

typedef void ga_nbgetput_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, Integer *nbhandle);

extern ga_nbgetput_fn_t pnga_nbget;
void
foilbase_pnga_nbget(ga_nbgetput_fn_t* real_pnga_nbget, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, Integer *nbhandle);

extern ga_nbgetput_fn_t pnga_nbput;
void
foilbase_pnga_nbput(ga_nbgetput_fn_t* real_pnga_nbput, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, Integer *nbhandle);

typedef void ga_nbacc_fn_t(Integer g_a, Integer *lo, Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle);
extern ga_nbacc_fn_t pnga_nbacc;
void
foilbase_pnga_nbacc(ga_nbacc_fn_t* real_pnga_nbacc, global_array_t* GA, Integer g_a, Integer *lo,
                    Integer *hi, void *buf, Integer *ld, void *alpha, Integer *nbhandle);

typedef void ga_nbwait_fn_t(Integer *nbhandle);
extern ga_nbwait_fn_t pnga_nbwait;
void
foilbase_pnga_nbwait(ga_nbwait_fn_t* real_pnga_nbwait, global_array_t* GA, Integer *nbhandle);

typedef void ga_brdcst_fn_t(Integer type, void *buf, Integer len, Integer originator);
extern ga_brdcst_fn_t pnga_brdcst;
void
foilbase_pnga_brdcst(ga_brdcst_fn_t* real_pnga_brdcst, global_array_t* GA, Integer type, void *buf,
                     Integer len, Integer originator);

typedef void ga_gop_fn_t(Integer type, void *x, Integer n, char *op);
extern ga_gop_fn_t pnga_gop;
void
foilbase_pnga_gop(ga_gop_fn_t* real_pnga_gop, global_array_t* GA, Integer type, void *x, Integer n,
                  char *op);

typedef void ga_sync_fn_t();
extern ga_sync_fn_t pnga_sync;
void
foilbase_pnga_sync(ga_sync_fn_t* real_pnga_sync, global_array_t* GA);

// TODO: ga_pgroup_sync
typedef void ga_pgroup_sync_fn_t(Integer grp_id);

// TODO: ga_pgroup_dgop
typedef void ga_pgroup_gop_fn_t(Integer p_grp, Integer type, void *x, Integer n, char *op);

#endif  // HPCRUN_SS_GA_OVERRIDES_H
