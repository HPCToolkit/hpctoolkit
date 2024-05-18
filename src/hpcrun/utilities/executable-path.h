// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef executable_path_h
#define executable_path_h

/*
 * function: executable_path
 *
 * purpose:
 *    search for an executable file that matches 'filename'
 *    relative to the current directory or a colon separated
 *    list of  paths
 *
 * return value:
 *   upon success, it will return the value supplied as path_name
 *
 *
 * NOTES:
 * 1. path_list is a colon separated list of paths to search
 *    for filename.
 * 2. path_name must point to a buffer of length PATH_MAX
 * 2. this routine can't allocate any memory, which
 *    makes the implementation a bit more complicated.
 */
char *executable_path(const char *filename, const char *path_list, char *executable_name);

#endif
