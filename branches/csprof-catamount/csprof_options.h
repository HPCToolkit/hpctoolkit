#ifndef CSPROF_OPTIONS_H
#define CSPROF_OPTIONS_H
#include <limits.h>

#include "sample_source.h"

#define CSPROF_PATH_SZ (PATH_MAX+1) /* path size */

/* represents options for the library */
typedef struct csprof_options_s {
  char out_path[CSPROF_PATH_SZ]; /* path for output */
  char addr_file[CSPROF_PATH_SZ]; /* path for "bad address" file */
  unsigned long mem_sz;       /* initial private memory size, bytes */
  char *event;                /* name of the event */
  unsigned long sample_period; /* when itimer is used */
  sample_source_t sample_source; /* what kind of event is used */
  unsigned int max_metrics;
} csprof_options_t;

#define CSPROF_OUT_PATH          "."
#define CSPROF_EVENT       "microseconds"
#define CSPROF_SMPL_PERIOD 1000UL /* microseconds */
#define CSPROF_MEM_SZ_INIT       32 * 1024 * 1024 /* FIXME: 1024 */

int csprof_options__init(csprof_options_t* x);
int csprof_options__fini(csprof_options_t* x);
int csprof_options__getopts(csprof_options_t* x);

#endif
