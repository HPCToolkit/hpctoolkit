// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: elf-hash.h
//
// Purpose:
//   interface to compute a crpytographic hash string for an elf binary
//
//***************************************************************************

#ifndef ELF_HASH_H
#define ELF_HASH_H

#ifdef __cplusplus
extern "C" {
#endif



//-----------------------------------------------------------------------------
// function:
//   elf_hash
//
// arguments:
//   filename: name of the ELF file to hash
//
// return value:
//   success: hash string
//   failure: NULL
//-----------------------------------------------------------------------------
char *
elf_hash
(
 const char *filename
);


#ifdef __cplusplus
};
#endif


#endif
