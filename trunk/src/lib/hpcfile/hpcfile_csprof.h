// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    hpcfile_csprof.h
//
// Purpose:
//    Low-level types and functions for reading/writing a call stack
//    profile from/to a binary file.
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef HPCFILE_CSPROF_H
#define HPCFILE_CSPROF_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "hpcfile_general.h"

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
//
// Types and functions for reading/writing a call stack tree from/to a
// binary file.  
//
// Basic format of HPC_CSPROF: see implementation file for more details
//   HPC_CSPROF header
//   List of data chunks describing profile metrics, etc.
//   HPC_CSTREE data
//
// Data in the file is stored in little-endian format, the ordering of
// IA-64 instructions and the default mode of an IA-64 processor.
//
//***************************************************************************

#define HPCFILE_CSPROF_MAGIC_STR     "HPC_CSPROF"
#define HPCFILE_CSPROF_MAGIC_STR_LEN 10 /* exclude '\0' */

#define HPCFILE_CSPROF_VERSION     "01.00"
#define HPCFILE_CSPROF_VERSION_LEN 5 /* exclude '\0' */

#define HPCFILE_CSPROF_ENDIAN 'l'

// ---------------------------------------------------------
// hpcfile_csprof_id_t: file identification.
//
// The size is 16 bytes and valid for both big/little endian ordering.
// Although these files will probably not have very long lives on
// disk, hopefully this will not need to be changed.
// ---------------------------------------------------------
typedef struct hpcfile_csprof_id_s {
  
  char magic_str[HPCFILE_CSPROF_MAGIC_STR_LEN]; 
  char version[HPCFILE_CSPROF_VERSION_LEN];
  char endian;  // 'b' or 'l' (currently, redundant info)
  
} hpcfile_csprof_id_t;

int hpcfile_csprof_id__init(hpcfile_csprof_id_t* x);
int hpcfile_csprof_id__fini(hpcfile_csprof_id_t* x);

int hpcfile_csprof_id__fread(hpcfile_csprof_id_t* x, FILE* fs);
int hpcfile_csprof_id__fwrite(hpcfile_csprof_id_t* x, FILE* fs);
int hpcfile_csprof_id__fprint(hpcfile_csprof_id_t* x, FILE* fs);


// ---------------------------------------------------------
// hpcfile_csprof_hdr_t:
// ---------------------------------------------------------
typedef struct hpcfile_csprof_hdr_s {

  hpcfile_csprof_id_t fid;

  // data information
  uint64_t num_data; // number of data chucks following header
  
} hpcfile_csprof_hdr_t;

int hpcfile_csprof_hdr__init(hpcfile_csprof_hdr_t* x);
int hpcfile_csprof_hdr__fini(hpcfile_csprof_hdr_t* x);

int hpcfile_csprof_hdr__fread(hpcfile_csprof_hdr_t* x, FILE* fs);
int hpcfile_csprof_hdr__fwrite(hpcfile_csprof_hdr_t* x, FILE* fs);
int hpcfile_csprof_hdr__fprint(hpcfile_csprof_hdr_t* x, FILE* fs);


/* hpcfile_csprof_metric_t */

typedef struct hpcfile_csprof_metric_s {
    char *metric_name;          /* name of the metric */
    uint64_t flags;             /* flags of the metric (async, etc.) */
    uint64_t sample_period;     /* sample period of the metric */
} hpcfile_csprof_metric_t;

int hpcfile_csprof_metric__init(hpcfile_csprof_metric_t *x);
int hpcfile_csprof_metric__fini(hpcfile_csprof_metric_t *x);

int hpcfile_csprof_metric__fread(hpcfile_csprof_metric_t *x, FILE *fs);
int hpcfile_csprof_metric__fwrite(hpcfile_csprof_metric_t *x, FILE *fs);
int hpcfile_csprof_metric__fprint(hpcfile_csprof_metric_t *x, FILE *fs);


// ---------------------------------------------------------
// hpcfile_csprof_data_t: used only for passing data; not actually
// part of the file format
// ---------------------------------------------------------

typedef struct hpcfile_csprof_data_s {
  char* target;               // name of profiling target
  uint32_t num_metrics;       // number of metrics recorded
  hpcfile_csprof_metric_t *metrics;
  
} hpcfile_csprof_data_t;

int hpcfile_csprof_data__init(hpcfile_csprof_data_t* x);
int hpcfile_csprof_data__fini(hpcfile_csprof_data_t* x);

int hpcfile_csprof_data__fprint(hpcfile_csprof_data_t* x, FILE* fs);


//**************************************************************************
// temp file for epoch read
//**************************************************************************
typedef struct ldmodule_s {
   char *name;
   uint64_t vaddr;
   uint64_t  mapaddr;
} ldmodule_t; 

typedef struct epoch_entry_s { 
  uint32_t num_loadmodule;
  ldmodule_t *loadmodule;
} epoch_entry_t;

typedef struct epoch_table_s {
       uint32_t num_epoch;
       epoch_entry_t *epoch_modlist;
} epoch_table_t; 


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

