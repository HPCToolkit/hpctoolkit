// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <string.h>

int main() {
  void* x = malloc(1024);
  memset(x, 42, 1024);

  // TODO: memalign (deprecated) / posix_memalign (POSIX) / aligned_alloc
  // TODO: valloc (deprecated)
  // TODO: calloc
  // TODO: free
  // TODO: realloc

  return 0;
}
