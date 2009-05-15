// -*-Mode: C;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    General and helper functions for reading/writing a HPC data
//    files from/to a binary file.
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_lean_hpcio_h
#define prof_lean_hpcio_h

//************************* System Include Files ****************************

#include <stdio.h>
#include <inttypes.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

// hpc_fread_leX: Reads 'X' number of little-endian bytes from the file
// stream 'fs', correctly orders them for the current architecture,
// and stores the result in 'val'. Returns the number of bytes read.
//
// hpc_fwrite_leX: Write 'X' number of bytes from 'val' to the
// little-endian file stream 'fs', correctly ordering the bytes before
// writing.  Returns the number of bytes written.
//
// hpc_fread_beX:  (not needed now)
// hpc_fwrite_beX: (not needed now)

size_t hpc_fread_le2(uint16_t* val, FILE* fs);
size_t hpc_fread_le4(uint32_t* val, FILE* fs);
size_t hpc_fread_le8(uint64_t* val, FILE* fs);
size_t hpc_fwrite_le2(uint16_t* val, FILE* fs);
size_t hpc_fwrite_le4(uint32_t* val, FILE* fs);
size_t hpc_fwrite_le8(uint64_t* val, FILE* fs);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcio_h */
