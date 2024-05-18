// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef findinstall_h
#define findinstall_h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * findinstall - Dynamically find installation path.  Inspired by
 * Perl's FindBin.
 *
 * Given the command (argv[0]) and the command's base name,
 * dynamically attempt to find where the command was executed from and
 * the installation path.
 *
 * N.B.: This routine stores the return value in statically allocated memory.
 *
 */

extern char*
findinstall(const char* cmd, const char* base_cmd);


#ifdef __cplusplus
}
#endif


#endif
