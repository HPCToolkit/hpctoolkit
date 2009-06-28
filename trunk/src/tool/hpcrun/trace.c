//*********************************************************************
// global includes 
//*********************************************************************

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>




//*********************************************************************
// local includes 
//*********************************************************************

#include "env.h"
#include "files.h"
#include "hpcio.h"
#include "monitor.h"
#include "pmsg.h"
#include "trace.h"
#include "thread_data.h"


//*********************************************************************
// type declarations
//*********************************************************************

typedef struct trecord_s {
  double time;
  unsigned int call_path_id;
} trecord_t;

#define TRECORD_SIZE (sizeof(double) + sizeof(unsigned int))

//*********************************************************************
// forward declarations 
//*********************************************************************

static void trace_file_validate(int valid, char *op);



//*********************************************************************
// local variables 
//*********************************************************************

static int tracing = 0;



//*********************************************************************
// interface operations
//*********************************************************************

int 
trace_isactive()
{
  return tracing;
}


void 
trace_init()
{
  if (getenv(CSPROF_OPT_TRACE)) {
    tracing = 1;
  }
}


void 
trace_open()
{
  if (tracing) {
    char trace_file[PATH_MAX];
    files_trace_name(trace_file, 0, PATH_MAX);
    thread_data_t *td = csprof_get_thread_data();
    td->trace_file = hpcio_fopen_w(trace_file, 0);
    trace_file_validate(td->trace_file != 0, "open");
  }
}


void
trace_append(unsigned int call_path_id)
{
  if (tracing) {
    struct timeval tv;
    int notime = gettimeofday(&tv, NULL);
    assert(notime == 0 && "in trace_append: gettimeofday failed!"); 
    double microtime = tv.tv_usec + tv.tv_sec * 1000000;

    thread_data_t *td = csprof_get_thread_data();
#if 1
    int written = hpcio_fwrite_be8((uint64_t*) &microtime, td->trace_file);
    written += hpcio_fwrite_be4((uint32_t*)&call_path_id, td->trace_file);
    trace_file_validate(written == (sizeof(double) + sizeof(int)), "append");
#else
#define NITEMS 1
    trecord_t record = { microtime, call_path_id };
    int written = fwrite(&record, TRECORD_SIZE, NITEMS, td->trace_file);
    trace_file_validate(written == NITEMS, "append");
#endif
  }
}


void
trace_close()
{
  if (tracing) {
    int ret;
    thread_data_t *td = csprof_get_thread_data();
    ret = fflush(td->trace_file);
    trace_file_validate(ret == 0, "flush");

    ret = hpcio_close(td->trace_file);
    trace_file_validate(ret == 0, "close");
    int rank = monitor_mpi_comm_rank();
    if (rank >= 0) {
      char old[PATH_MAX];
      char new[PATH_MAX];
      files_trace_name(old, 0, PATH_MAX);
      files_trace_name(new, rank, PATH_MAX);
      rename(old, new);
    }
  }
}



//*********************************************************************
// private operations
//*********************************************************************


static void
trace_file_validate(int valid, char *op)
{
  if (!valid) {
    EMSG("unable to %s trace file\n", op);
    monitor_real_abort();
  }
}
