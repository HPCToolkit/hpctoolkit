// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
#include "cii20-assert.h"
#include "except.h"
#include "fmt.h"
#include "cii20-mem.h"

#define T Fmt_T

struct buf {
  char *buf;
  char *bp;
  int size;
};

#define pad(n,c) do { int nn = (n); \
	while (nn-- > 0) \
		put((c), cl); } while (0)
static void
cvt_s(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  char *str = va_arg(box->ap, char *);
  assert(str);
  Fmt_puts(str, strlen(str), put, cl, flags,
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
  Fmt_putd(p, (buf + sizeof buf) - p, put, cl, flags,
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
  Fmt_putd(p, (buf + sizeof buf) - p, put, cl, flags,
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
  Fmt_putd(p, (buf + sizeof buf) - p, put, cl, flags,
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
  Fmt_putd(p, (buf + sizeof buf) - p, put, cl, flags,
           width, precision);
}

static void
cvt_p(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  uintptr_t m = (uintptr_t)va_arg(box->ap, void*);
  char buf[43];
  char *p = buf + sizeof buf;
  precision = INT_MIN;
  do
    *--p = "0123456789abcdef"[m&0xf];
  while ((m>>= 4) != 0);
  Fmt_putd(p, (buf + sizeof buf) - p, put, cl, flags,
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

//
// unit testing stuff for cvt
//
#if 0

#define chk_cvt(fp, prec, is_neg) \
do {                                                                           \
  neg = is_neg;                                                                \
  int rv = cvt(nbuf, FPBUF_LEN, fp, prec, &neg);                               \
  char* _t = *nbuf ? nbuf : nbuf+1;                                            \
  _t[rv] = '\0';                                                               \
  printf("float fp(@ prec:%d) %f = %s\n", prec, fp, _t);                       \
} while(0)

  chk_cvt(0.99, 1, false);
  chk_cvt(3.1415, DEFPREC, false);
  chk_cvt(3.1415e10, 3, false);
  chk_cvt(3.1415, 3, false);

#endif // unit testing

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
  Fmt_putd(_t, strlen(_t), put, cl, flags,
           width, precision);
}
static void
old_cvt_f(int code, va_list_box *box,
      int put(int c, void *cl), void *cl,
      unsigned char flags[], int width, int precision)
{
  char buf[DBL_MAX_10_EXP+1+1+99+1];
  if (precision < 0)
    precision = 6;
  if (code == 'g' && precision == 0)
    precision = 1;
  {
    static char fmt[] = "%.dd?";
    assert(precision <= 99);
    fmt[4] = code;
    fmt[3] =      precision%10 + '0';
    fmt[2] = (precision/10)%10 + '0';
    sprintf(buf, fmt, va_arg(box->ap, double));
  }
  Fmt_putd(buf, strlen(buf), put, cl, flags,
           width, precision);
}

const Except_T Fmt_Overflow = { "Formatting Overflow" };

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

static const char *Fmt_flags = "-+ 0";

static int
outc(int c, void *cl)
{
  FILE *f = cl;
  return putc(c, f);
}

static int
insert(int c, void *cl)
{
  struct buf *p = cl;
  if (p->bp >= p->buf + p->size){
    RAISE(Fmt_Overflow);
  }
  *p->bp++ = c;
  return c;
}

static int
append(int c, void *cl)
{
  struct buf *p = cl;
  if (p->bp >= p->buf + p->size) {
    RESIZE(p->buf, 2*p->size);
    p->bp = p->buf + p->size;
    p->size *= 2;
  }
  *p->bp++ = c;
  return c;
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


void
Fmt_puts(const char *str, int len,
         int put(int c, void *cl), void *cl,
         unsigned char flags[], int width, int precision)
{
  assert(str);
  assert(len >= 0);
  assert(flags);
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
Fmt_fmt(int put(int c, void *), void *cl,
	const char *fmt, ...)
{
  va_list_box box;
  va_start(box.ap, fmt);
  Fmt_vfmt(put, cl, fmt, &box);
  va_end(box.ap);
}

void
Fmt_print(const char *fmt, ...)
{
  va_list_box box;
  va_start(box.ap, fmt);
  Fmt_vfmt(outc, stdout, fmt, &box);
  va_end(box.ap);
}

void
Fmt_fprint(FILE *stream, const char *fmt, ...)
{
  va_list_box box;
  va_start(box.ap, fmt);
  Fmt_vfmt(outc, stream, fmt, &box);
  va_end(box.ap);
}

int
Fmt_sfmt(char *buf, int size, const char *fmt, ...)
{
  int len;
  va_list_box box;
  va_start(box.ap, fmt);
  len = Fmt_vsfmt(buf, size, fmt, &box);
  va_end(box.ap);
  return len;
}

int
Fmt_vsfmt(char *buf, int size, const char *fmt,
          va_list_box *box)
{
  struct buf cl;
  assert(buf);
  assert(size > 0);
  assert(fmt);
  cl.buf = cl.bp = buf;
  cl.size = size;
  Fmt_vfmt(insert, &cl, fmt, box);
  insert(0, &cl);
  return cl.bp - cl.buf - 1;
}

char*
Fmt_string(const char *fmt, ...)
{
  char *str;
  va_list_box box;
  assert(fmt);	
  va_start(box.ap, fmt);
  str = Fmt_vstring(fmt, &box);
  va_end(box.ap);
  return str;
}

char*
Fmt_vstring(const char *fmt, va_list_box *box)
{
  struct buf cl;
  assert(fmt);
  cl.size = 256;
  cl.buf = cl.bp = ALLOC(cl.size);
  Fmt_vfmt(append, &cl, fmt, box);
  append(0, &cl);
  return RESIZE(cl.buf, cl.bp - cl.buf);
}

int
Fmt_ns(char* buf, size_t len, const char* fmt, ...)
{
  int nchars;
  va_list_box box;
  va_list_box_start(box, fmt);
  nchars = Fmt_vns(buf, len, fmt, &box);
  va_list_box_end(box);
  return nchars;
}

int
Fmt_vns(char* buf, size_t len, const char* fmt, va_list_box* box)
{
  struct buf cl = {
    .buf  = buf,
    .bp   = buf,
    .size = len,
  };

  Fmt_vfmt(fixed_len_insert, &cl, fmt, box);
  fixed_len_insert((int) '\0', &cl);
  return cl.bp - cl.buf -1;
}

void
Fmt_vfmt(int put(int c, void *cl), void *cl,
         const char *fmt, va_list_box *box)
{
  assert(put);
  assert(fmt);
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
            assert(flags[c] < 255);
            flags[c]++;
          }
        }
        if (*fmt == '*' || isdigit(*fmt)) {
          int n;
          if (*fmt == '*') {
            n = va_arg(box->ap, int);
            assert(n != INT_MIN);
            fmt++;
          } else
            for (n = 0; isdigit(*fmt); fmt++) {
              int d = *fmt - '0';
              assert(n <= (INT_MAX - d)/10);
              n = 10*n + d;
            }
          width = n;
        }
        if (*fmt == '.' && (*++fmt == '*' || isdigit(*fmt))) {
          int n;
          if (*fmt == '*') {
            n = va_arg(box->ap, int);
            assert(n != INT_MIN);
            fmt++;
          } else
            for (n = 0; isdigit(*fmt); fmt++) {
              int d = *fmt - '0';
              assert(n <= (INT_MAX - d)/10);
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
Fmt_register(int code, T newcvt)
{
  T old;
  assert(0 < code
         && code < (int)(sizeof (cvt)/sizeof (cvt[0])));
  old = cvt[code];
  cvt[code] = newcvt;
  return old;
}

void
Fmt_putd(const char *str, int len,
         int put(int c, void *cl), void *cl,
         unsigned char flags[], int width, int precision)
{
  int sign;
  assert(str);
  assert(len >= 0);
  assert(flags);
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
