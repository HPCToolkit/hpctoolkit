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

#ifndef FMT_INCLUDED
#define FMT_INCLUDED
#include <stdarg.h>
#include <stdio.h>

typedef struct va_list_box {
	va_list ap;
} va_list_box;

#define va_list_box_start(box, arg)  va_start(box.ap, arg)
#define va_list_box_end(box)         va_end(box.ap)
#define va_list_boxp_start(box, arg) va_start(box->ap, arg)
#define va_list_boxp_end(box)        va_end(box->ap)

#define T Fmt_T
typedef void (*T)(int code, va_list_box *box,
	int put(int c, void *cl), void *cl,
	unsigned char flags[256], int width, int precision);

extern void hpcrun_msg_fmt (int put(int c, void *cl), void *cl,
	const char *fmt, ...);
extern void hpcrun_msg_vfmt(int put(int c, void *cl), void *cl,
	const char *fmt, va_list_box *box);

extern int hpcrun_msg_ns(char* buf, size_t len, const char* fmt, ...);
extern int hpcrun_msg_vns(char* buf, size_t len, const char* fmt, va_list_box* box);

extern T hpcrun_msg_register(int code, T cvt);
extern void hpcrun_msg_putd(const char *str, int len,
	int put(int c, void *cl), void *cl,
	unsigned char flags[256], int width, int precision);
extern void hpcrun_msg_puts(const char *str, int len,
	int put(int c, void *cl), void *cl,
	unsigned char flags[256], int width, int precision);
#undef T
#endif
