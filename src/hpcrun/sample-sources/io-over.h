// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_SS_IO_OVER_H
#define HPCRUN_SS_IO_OVER_H

#include <stddef.h>
#include <unistd.h>
#include <stdio.h>

typedef ssize_t read_fn_t(int, void *, size_t);
typedef ssize_t write_fn_t(int, const void *, size_t);
typedef size_t  fread_fn_t(void *, size_t, size_t, FILE *);
typedef size_t  fwrite_fn_t(const void *, size_t, size_t, FILE *);

extern ssize_t foilbase_read(read_fn_t* real_fn, int fd, void *buf, size_t count);
extern ssize_t foilbase_write(write_fn_t* real_fn, int fd, const void *buf, size_t count);
extern size_t foilbase_fread(fread_fn_t* real_fn, void *ptr, size_t size, size_t count, FILE *stream);
extern size_t foilbase_fwrite(fwrite_fn_t* real_fn, const void *ptr, size_t size, size_t count, FILE *stream);


#endif  // HPCRUN_SS_IO_OVER_H
