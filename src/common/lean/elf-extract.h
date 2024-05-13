// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: elf-extract.h
//
// Purpose:
//   determine section location and size in elf binary
//
//***************************************************************************

#ifndef __elf_extract_h__
#define __elf_extract_h__

#include <stdbool.h>

bool
elf_section_info
(
 unsigned char *binary,
 size_t binary_size,
 const char *section_name,
 unsigned char **section,
 size_t *section_size
);


#endif
