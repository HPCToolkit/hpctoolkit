// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>

int main() {
  FILE* f = fopen("/proc/self/exe", "rb");
  if (f == NULL) {
    perror("Skipping test: failed to open /proc/self/exe");
    return 77;
  }

  char buf[4];
  if (fread(buf, sizeof buf, 1, f) < 1) {
    if (feof(f)) {
      fprintf(stderr, "Unexpected EOF while reading data\n");
      return 1;
    }
    if (ferror(f)) {
      fprintf(stderr, "I/O while reading file\n");
      return 1;
    }
  }

  // TODO: fwrite
  // TODO: read (POSIX)
  // TODO: write (POSIX)

  return 0;
}
