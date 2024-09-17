// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_SS_IO_OVER_H
#define HPCRUN_SS_IO_OVER_H

#include "../foil/libc.h"

extern ssize_t hpcrun_read(int fd, void *buf, size_t count,
                             const struct hpcrun_foil_appdispatch_libc_io* dispatch);
extern ssize_t hpcrun_write(int fd, const void *buf, size_t count,
                              const struct hpcrun_foil_appdispatch_libc_io* dispatch);
extern size_t hpcrun_fread(void *ptr, size_t size, size_t count, FILE *stream,
                             const struct hpcrun_foil_appdispatch_libc_io* dispatch);
extern size_t hpcrun_fwrite(const void *ptr, size_t size, size_t count, FILE *stream,
                              const struct hpcrun_foil_appdispatch_libc_io* dispatch);


#endif  // HPCRUN_SS_IO_OVER_H
