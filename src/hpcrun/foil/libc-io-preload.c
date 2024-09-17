// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "libc-private.h"

#include <assert.h>
#include <malloc.h>
#include <stdarg.h>
#include <threads.h>

static struct hpcrun_foil_appdispatch_libc_io dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_libc_io){
      .read = foil_dlsym("read"),
      .write = foil_dlsym("write"),
      .fread = foil_dlsym("fread"),
      .fwrite = foil_dlsym("fwrite"),
  };
}

static const struct hpcrun_foil_appdispatch_libc_io* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API ssize_t read(int fd, void* buf, size_t count) {
  return hpcrun_foil_fetch_hooks_libc_dl()->read(fd, buf, count, dispatch());
}

HPCRUN_EXPOSED_API ssize_t write(int fd, const void* buf, size_t count) {
  return hpcrun_foil_fetch_hooks_libc_dl()->write(fd, buf, count, dispatch());
}

HPCRUN_EXPOSED_API size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
  return hpcrun_foil_fetch_hooks_libc_dl()->fread(ptr, size, count, stream, dispatch());
}

HPCRUN_EXPOSED_API size_t fwrite(const void* ptr, size_t size, size_t count,
                                 FILE* stream) {
  return hpcrun_foil_fetch_hooks_libc_dl()->fwrite(ptr, size, count, stream,
                                                   dispatch());
}
