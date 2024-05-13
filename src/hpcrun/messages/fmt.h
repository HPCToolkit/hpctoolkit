// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
