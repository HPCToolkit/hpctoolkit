// -*-Mode: C++;-*- // technically C99

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

//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <math.h>

//*****************************************************************************
// local includes 
//*****************************************************************************

#include "fmt.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define T Fmt_T

#define pad(n,c) do { int nn = (n); \
	while (nn-- > 0) \
		put((c), cl); } while (0)

//
// Useful constants for floating point manipulation
//
//

//
// 308 decimal digits = max # of digits to left of decimal point (when not using scientific notation
//
//
#define	MAXEXP		308

//
// 128 bit fraction takes up 39 decimal digits; max reasonable precision
//
#define	MAXFRACT	39

//
// default precisions
//
#define	DEFPREC		6
#define	DEFLPREC	14

// max digits to left + max digits to right + decimal point
#define	FPBUF_LEN	(MAXEXP+MAXFRACT+1)

//***********************************
// structures and data types
//***********************************


struct buf {
  char *buf;
  char *bp;
  int size;
};

//***********************************
// local data
//***********************************

static const char *Fmt_flags = "-+ 0";

//***********************************
// private operations
//***********************************

static void
cvt_s(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  char *str = va_arg(box->ap, char *);
  if (str == NULL) { str = "(null)"; }

  hpcrun_msg_puts(str, strlen(str), put, cl, flags,
           width, precision);
}

static void
cvt_d(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  long val;

  if ( flags['l'] ) {
    val = va_arg(box->ap, long);
  }
  else {
    val = va_arg(box->ap, int);
  }
  unsigned long m;
  char buf[86];

  char *p = buf + sizeof buf;
  if (val == INT_MIN)
    m = INT_MAX + 1U;
  else if (val < 0)
    m = -val;
  else
    m = val;
  do 
    *--p = m%10 + '0';
  while ((m /= 10) > 0);
  if (val < 0)
    *--p = '-';
  hpcrun_msg_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_u(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  unsigned long m;

  if ( flags['l'] ) {
    m = va_arg(box->ap, unsigned long);
  }
  else {
    m = va_arg(box->ap, unsigned int);
  }
  char buf[143];
  char *p = buf + sizeof buf;
  do
    *--p = m%10 + '0';
  while ((m /= 10) > 0);
  hpcrun_msg_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_o(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  unsigned long m;

  if ( flags['l'] ) {
    m = va_arg(box->ap, unsigned long);
  }
  else {
    m = va_arg(box->ap, unsigned int);
  }
  char buf[143];

  char *p = buf + sizeof buf;
  do
    *--p = (m&0x7) + '0';
  while ((m>>= 3) != 0);
  hpcrun_msg_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_x(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  unsigned long m;

  if ( flags['l'] ) {
    m = va_arg(box->ap, unsigned long);
  }
  else {
    m = va_arg(box->ap, unsigned int);
  }
  char buf[143];

  char *p = buf + sizeof buf;
  do
    *--p = "0123456789abcdef"[m&0xf];
  while ((m>>= 4) != 0);
  hpcrun_msg_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_p(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  uintptr_t m = (uintptr_t)va_arg(box->ap, void*);
  char buf[43];
  char* p = buf + sizeof buf;
  precision = INT_MIN;
  do
    *--p = "0123456789abcdef"[m&0xf];
  while ((m>>= 4) != 0);
  put('0', cl);
  put('x', cl);
  hpcrun_msg_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_c(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  if (width == INT_MIN)
    width = 0;
  if (width < 0) {
    flags['-'] = 1;
    width = -width;
  }
  if (!flags['-'])
    pad(width - 1, ' ');
  put((unsigned char)va_arg(box->ap, int), cl);
  if ( flags['-'])
    pad(width - 1, ' ');
}

//
// rounding routine used by f.p. conversion
// NOTE: this routine rounds up by using BCD arithmetic (digits are characters).
//
static void
roundit_f(double num, char* start, char* end, bool* neg)
{
  double tmp;
  modf(num * 10., &tmp);

  if (tmp > 4.) {
    for (;; --end) {
      if (*end == '.') {
        --end;
      }
      if (++*end <= '9') {
        break;
      }
      *end = '0';
      if (end == start) {
        *--end = '1';
        --start;
        break;
      }
    }
  }
  else if (*neg) {
    for (;; --end) {
      if (*end == '.') {
        --end;
      }
      if (*end != '0') {
        break;
      }
      if (end == start) {
        *neg = false;
      }
    }
  }
}

//
// char string rep of double
// buf[buflen] gets rep of num to specified precision
//
static int
help_cvt_f(char* buf, size_t buflen, double num, int prec, bool* neg)
{
  static char digits[] = "0123456789";

  double integer;
  int expnt = 0;
  char* bufend = buf+buflen-1;
  buf[0] = '\0';

  char* p = bufend;
  char* t = buf+1; // allow 1st slot for rounding: 9.9 rounds up to 10.

  double frac = modf(num, &integer);
  for (; integer; ++expnt) {
    double tmp = modf(integer / 10., &integer);
    *p-- = digits[(int) ((tmp + .01) * 10.)];
  }
  // case f
  if (expnt) {
    memmove(t, p+1, bufend-p);
    t += bufend-p;
  }
  else {
    *t++ = '0';
  }
  if (prec) {
    *t++ = '.';
  }
  if (frac) {
    if (prec) {
      do {
        frac = modf(frac * 10., &integer);
        *t++ = digits[(int) integer];
      } while (--prec && frac);
    }
    if (frac) {
      roundit_f(frac, buf+1, t-1, neg);
    }
  }
  memset(t, '0', prec);
  t += prec;
  return t - buf - 1;
}

static void
cvt_f(int code, va_list_box* box,
      int put(int c, void* cl), void* cl,
      unsigned char flags[], int width, int precision)
{
  char _nbuf[FPBUF_LEN+1]; // add 1 for sign
  bool neg = false;

  if (precision < 0){
    precision = 6;
  }
  double num = va_arg(box->ap, double);
  if (num < 0) {
    neg = true;
    num = - num;
  }
  char* nbuf = _nbuf+1;
  int rv     = help_cvt_f(nbuf, FPBUF_LEN, num, precision, &neg);
  char* _t   = *nbuf ? nbuf : nbuf+1;
  _t[rv]     = '\0';
  if (neg) {
    _t--;
    _t[0] = '-';
    rv++;
  }
  hpcrun_msg_putd(_t, strlen(_t), put, cl, flags,
           width, precision);
}

static int
fixed_len_insert(int c, void* cl)
{
  struct buf* p = (struct buf*) cl;
  if (! (p->bp >= p->buf + p->size)) {
    *p->bp++ = (char) c;
  }

  return c;
}

//*****************************************************************************
// local conversion character table
//*****************************************************************************

static T cvt[256] = {
 /*   0-  7 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*   8- 15 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  16- 23 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  24- 31 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  32- 39 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  40- 47 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  48- 55 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  56- 63 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  64- 71 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  72- 79 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  80- 87 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  88- 95 */ 0,     0, 0,     0,     0,     0,     0,     0,
 /*  96-103 */ 0,     0, 0, cvt_c, cvt_d, cvt_f, cvt_f, cvt_f,
 /* 104-111 */ 0,     0, 0,     0,     0,     0,     0, cvt_o,
 /* 112-119 */ cvt_p, 0, 0, cvt_s,     0, cvt_u,     0,     0,
 /* 120-127 */ cvt_x, 0, 0,     0,     0,     0,     0,     0
};

//*****************************************************************************
// interface operations
//*****************************************************************************

void
hpcrun_msg_puts(const char *str, int len,
         int put(int c, void *cl), void *cl,
         unsigned char flags[], int width, int precision)
{
  if (width == INT_MIN)
    width = 0;
  if (width < 0) {
    flags['-'] = 1;
    width = -width;
  }
  if (precision >= 0)
    flags['0'] = 0;
  if (precision >= 0 && precision < len)
    len = precision;
  if (!flags['-'])
    pad(width - len, ' ');
  {
    int i;
    for (i = 0; i < len; i++)
      put((unsigned char)*str++, cl);
  }
  if ( flags['-'])
    pad(width - len, ' ');
}

void
hpcrun_msg_fmt(int put(int c, void *), void *cl,
	const char *fmt, ...)
{
  va_list_box box;
  va_start(box.ap, fmt);
  hpcrun_msg_vfmt(put, cl, fmt, &box);
  va_end(box.ap);
}

int
hpcrun_msg_ns(char* buf, size_t len, const char* fmt, ...)
{
  int nchars;
  va_list_box box;
  va_list_box_start(box, fmt);
  nchars = hpcrun_msg_vns(buf, len, fmt, &box);
  va_list_box_end(box);
  return nchars;
}

int
hpcrun_msg_vns(char* buf, size_t len, const char* fmt, va_list_box* box)
{
  struct buf cl = {
    .buf  = buf,
    .bp   = buf,
    .size = len,
  };

  hpcrun_msg_vfmt(fixed_len_insert, &cl, fmt, box);
  fixed_len_insert((int) '\0', &cl);
  return cl.bp - cl.buf -1;
}

void
hpcrun_msg_vfmt(int put(int c, void *cl), void *cl,
         const char *fmt, va_list_box *box)
{
  while (*fmt)
    if (*fmt != '%' || *++fmt == '%')
      put((unsigned char)*fmt++, cl);
    else
      {
        unsigned char c, flags[256];
        int width = INT_MIN, precision = INT_MIN;
        memset(flags, '\0', sizeof flags);
        if (Fmt_flags) {
          unsigned char c = *fmt;
          for ( ; c && strchr(Fmt_flags, c); c = *++fmt) {
            flags[c]++;
          }
        }
        if (*fmt == '*' || isdigit(*fmt)) {
          int n;
          if (*fmt == '*') {
            n = va_arg(box->ap, int);
            fmt++;
          } else
            for (n = 0; isdigit(*fmt); fmt++) {
              int d = *fmt - '0';
              n = 10*n + d;
            }
          width = n;
        }
        if (*fmt == '.' && (*++fmt == '*' || isdigit(*fmt))) {
          int n;
          if (*fmt == '*') {
            n = va_arg(box->ap, int);
            fmt++;
          } else
            for (n = 0; isdigit(*fmt); fmt++) {
              int d = *fmt - '0';
              n = 10*n + d;
            }
          precision = n;
        }
        c = *fmt++;
        if (c == 'l') {
          flags[c] = 1;
          c = *fmt++;
        }
        if (cvt[c]){
          (*cvt[c])(c, box, put, cl, flags, width, precision);
        }
      }
}

T
hpcrun_msg_register(int code, T newcvt)
{
  T old;

  old = cvt[code];
  cvt[code] = newcvt;
  return old;
}

void
hpcrun_msg_putd(const char *str, int len,
         int put(int c, void *cl), void *cl,
         unsigned char flags[], int width, int precision)
{
  int sign;

  if (width == INT_MIN)
    width = 0;
  if (width < 0) {
    flags['-'] = 1;
    width = -width;
  }
  if (precision >= 0)
    flags['0'] = 0;
  if (len > 0 && (*str == '-' || *str == '+')) {
    sign = *str++;
    len--;
  } else if (flags['+'])
    sign = '+';
  else if (flags[' '])
    sign = ' ';
  else
    sign = 0;
  { int n;
    if (precision < 0)
      precision = 1;
    if (len < precision)
      n = precision;
    else if (precision == 0 && len == 1 && str[0] == '0')
      n = 0;
    else
      n = len;
    if (sign)
      n++;
    if (flags['-']) {
      if (sign)
        put(sign, cl);
    } else if (flags['0']) {
      if (sign)
        put(sign, cl);
      pad(width - n, '0');
    } else {
      pad(width - n, ' ');
      if (sign)
        put(sign, cl);
    }
    pad(precision - len, '0');
    {
      int i;
      for (i = 0; i < len; i++)
        put((unsigned char)*str++, cl);
    }
    if (flags['-'])
      pad(width - n, ' '); }
}
