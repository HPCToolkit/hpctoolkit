// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//****************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//
//
//****************************************************************************

#ifndef support_CStrUtil
#define support_CStrUtil

/*************************** System Include Files ***************************/

/**************************** User Include Files ****************************/

/*************************** Forward Declarations ***************************/

/****************************************************************************/

extern int   STREQ(const char* x, const char* y);

extern char* ssave(const char *const str);
extern void  sfree(char* str);
extern char* nssave(int n, const char *const s1, ...);
extern int   find(char s1[], char s2[]);
extern int   char_count(char s1[], char s2[]);
extern int   hash_string(const char* string, int size);
extern char* strlower(char* string);
extern char* strupper(char* string);
extern char  to_lower(char c);

// Converts an integer to its ascii representation.
extern void itoa  (long n, char a[]);
extern void utoa  (unsigned long n, char a[]);

// Converts an unsigned long (e.g. ptr) to its hex representation.
extern void ultohex  (unsigned long n, char a[]);

#endif
