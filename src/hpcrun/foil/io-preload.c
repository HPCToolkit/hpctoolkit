// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../sample-sources/io-over.h"

HPCRUN_EXPOSED ssize_t read(int fd, void* buf, size_t count) {
  LOOKUP_FOIL_BASE(base, read);
  FOIL_DLSYM(real, read);
  return base(real, fd, buf, count);
}

HPCRUN_EXPOSED ssize_t write(int fd, const void* buf, size_t count) {
  LOOKUP_FOIL_BASE(base, write);
  FOIL_DLSYM(real, write);
  return base(real, fd, buf, count);
}

HPCRUN_EXPOSED size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
  LOOKUP_FOIL_BASE(base, fread);
  FOIL_DLSYM(real, fread);
  return base(real, ptr, size, count, stream);
}

HPCRUN_EXPOSED size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
  LOOKUP_FOIL_BASE(base, fwrite);
  FOIL_DLSYM(real, fwrite);
  return base(real, ptr, size, count, stream);
}
