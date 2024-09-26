// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "papi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int retval;
  int EventSet = PAPI_NULL;

  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    fprintf(stderr, "Error! PAPI_library_init returned %d\n", retval);
    return 2;
  }

  retval = PAPI_create_eventset(&EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error! PAPI_create_eventset returned %d\n", retval);
    return 2;
  }

  for (int i = 1; i < argc; i++) {
    retval = PAPI_add_named_event(EventSet, argv[i]);
    if (retval != PAPI_OK) {
      printf("Failed: %s: %s\n", argv[i], PAPI_strerror(retval));
      return 1;
    } else {
      printf("Supported: %s\n", argv[i]);
    }
  }

  return 0;
}
