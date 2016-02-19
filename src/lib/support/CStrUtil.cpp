// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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

/*************************** System Include Files ***************************/

#ifdef NO_STD_CHEADERS
# include <stdarg.h>
# include <ctype.h>
# include <string.h>
#else
# include <cstdarg>
# include <cctype>
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

/**************************** User Include Files ****************************/

#include "CStrUtil.h"

/**************************** Forward Declarations **************************/

/****************************************************************************/

/* #define STREQ(x,y) ((*(x) == *(y)) && !strcmp((x), (y))) */

int
STREQ(const char* x, const char* y)
{
  return ((*(x) == *(y)) && !strcmp((x), (y)));
}


char*
ssave(const char* const str)
{
  char* nstr = new char[strlen(str)+1];
  strcpy(nstr, str);
  return nstr;
}


void
sfree(char *str)
{
  delete[] str;
  return;
}


void
smove(char **old, char *fresh)
{
  sfree(*old);
  *old = ssave(fresh);
  return;
}


/*
 *  strcpye -     like strcpy, but returns a pointer
 *                to the null that terminates s1.
 */
static char*
strcpye(register char* s1, register char* s2)
{
  while ( (*s1++ = *s2++) );
  return --s1;
}


/*
 * nssave(n,s1,...,sn) - concatenate n strings into a dynamically allocated
 * blob, and return a pointer to the result. "n" must be equal to the # of
 * strings. The returned pointer should be freed with sfree().
 */
char*
nssave(int n, const char* const s1, ...)
{
  va_list ap;
  int nb = 0; /* the length of the result (bytes to allocate) */
  char* nstr; /* the result */
  char* tstr; /* temporary */

  /* Compute the length of the result */
  va_start(ap, s1);
  {
    nb = strlen(s1);
    for (int i = 0; i < n-1; i++) nb += strlen(va_arg(ap, char*));
  }
  va_end(ap);

  tstr = new char[nb+1];

  /* Concat them all together into the new space. */
  va_start(ap, s1);
  {
    char *loc = tstr;
    loc = strcpye(loc, (char*)s1);
    for (int i = 0; i < n-1; i++) loc = strcpye(loc, va_arg(ap, char*));
  }
  va_end(ap);

  nstr = ssave(tstr);

  delete[] tstr;

  return nstr;
}


/*
 *  locate the first occurrence of string s2 within s1.
 *  behaves properly for null s1.
 *  returns -1 for no match.
 */
int
find(char s1[], char s2[])
{
  int l1, l2, i, j;
  bool match;

  l1 = strlen(s1);
  l2 = strlen(s2);
  for (i = 0; i <= l1-l2; i++)
    {
      match = true;
      for (j = 0; match && (j < l2); j++) if (s1[i+j] != s2[j]) match = false;
      if (match) return i;
    }

  return -1;
}


/*
 * counts occurrences of characters in s2 within s1.
 */
int
char_count(char s1[], char s2[])
{
  int l1, l2, i, j, count;
  char c1;

  l1 = strlen(s1);
  l2 = strlen(s2);
  count = 0;
  for (i = 0; i < l1; i++)
    {
       c1 = s1[i];
       for (j = 0; j < l2; j++) if (c1 == s2[j]) count++;
    }

  return count;
}


int
hash_string(register const char* string, int size)
{
  register unsigned int result = 0;

  if (*string == '\0')
    return result;  /* no content */

  const char* stringend = strchr(string, '\0') - 1; /* address of last char */
  int step = ((stringend - string) >> 2) + 1;  /* gives <= 4 samples */
  while(stringend >= string)
    {
      result <<= 7;
      result |= (*(unsigned char*) stringend) & 0x3F;
      stringend -= step;
    }

  return (result % size);
}


char*
strlower (char *string)
{
  char* s = string;
  char c;

  while ((c = *s)) {
    if (isupper(c)) {
      *s = (char) tolower(c);
    }
    s++;
  }

  return string;
}


char*
strupper (char* string)
{
  char* s = string;
  char c;

  while ((c = *s)) {
    if (islower(c)) {
      *s = (char) toupper(c);
    }
    s++;
  }

  return string;
}


char
to_lower(char c)
{
  if (isupper(c)) {
    return (char) tolower(c);
  }
  else {
    return c;
  }
}


/* Converts an integer to its ascii representation */
void
itoa(long n, char a[])
{
  char* aptr;

  if (n < 0) {
    a[0] = '-';
    aptr = a+1;
    n = -n;
  }
  else
    aptr = a;
  utoa((unsigned long) n, aptr);
}


void
utoa(unsigned long n, char a[])
{
  char *aptr = a;
  int i=0;
  while (n > 0) {
    aptr[i++] = '0' + n%10;
    n = n / 10;
  }
  if (!i)
    aptr[i++] = '0';

  /* swap aptr[] end-for-end */
  for (int j=0; j<i/2; j++) {
    aptr[i] = aptr[j];
    aptr[j] = aptr[i-j-1];
    aptr[i-j-1] = aptr[i];
  }
  aptr[i] = '\0';
}


/* converts 64 (or less) bit pointers into hex "strings" */
void
ultohex (unsigned long n, char a[])
{
  int i;

  a[0] = '0';
  a[1] = 'x';
  for (i=2; i<18; i++) {
    a[i] = '0';
  }
  a[18] = '\0';

  i = 17;
  do {
    a[i--] = n % 16 + '0';
    if ( i<1 ) break;  /* ack, why are we running out of space?? */
  } while ((n /= 16) > 0);
}
