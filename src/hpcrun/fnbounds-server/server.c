// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// The server side of the new fnbounds server.  Hpcrun creates a pipe
// and then forks and execs hpcfnbounds in server mode (-s).  The file
// descriptors are passed as command-line arguments.
//
// This file implements the server side of the pipe.  Read messages
// over the pipe, process fnbounds queries and write the answer
// (including the array of addresses) back over the pipe.  The file
// 'syserv-mesg.h' defines the API for messages over the pipe.
//
// Notes:
// 1. The server only computes fnbounds queries, not general calls to
// system().
//
// 2. Catch SIGPIPE.  Writing to a pipe after the other side has
// exited triggers a SIGPIPE and terminates the process.  If this
// happens, it probably means that hpcrun has prematurely exited.
// So, catch SIGPIPE in order to write a more useful error message.
//
// 3. It's ok to write error messages to stderr.  After hpcrun forks,
// it dups the hpcrun log file fd onto stdout and stderr so that any
// output goes to the log file.
//
// 4. The server runs outside of hpcrun and libmonitor.
//
//***************************************************************************

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <err.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fnbounds.h"

#include "syserv-mesg.h"

#define ADDR_SIZE   (256 * 1024)

#define SUCCESS   0
#define FAILURE  -1
#define END_OF_FILE  -2

// HACK
#define ENABLED(x) (1)
#define TMSG(f, ...) do { fprintf(stderr, #f ": " __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#define ETMSG(f, ...) do { fprintf(stderr, #f ": " __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define hpcrun_abort(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)

static uint64_t        init_server(int, int);
static void    do_query(int fdin, int fdout, struct syserv_mesg *);
static void  send_funcs(int fdout, const FnboundsResponse* response, const char* xname);

static void    signal_handler_init();
static int     read_all(int, void*, size_t);
static int     write_all(int, const void*, size_t);
static int     read_mesg(int fdin, struct syserv_mesg *mesg);
static int     write_mesg(int fdout, int32_t type, int64_t len);
static void    signal_handler(int);


int
main(int argc, char **argv, char **envp)
{
  int i;
  char  **p;
  char  *ret;

  int disable_init = 0;
  p = argv;
  p++;
  for (i = 1; i < argc; i ++) {
    if ( strcmp (*p, "-s") == 0 ) {
      // code to initialize as a server  "-s <infd> <outfd>"
      if ((i+2) > argc) {
        // argument count is wrong
        hpcrun_abort("hpcrun: hpcfnbounds server invocation too few arguments\n");
      }
     p++;
     int _infd = atoi (*p);
     p++;
     int _outfd = atoi (*p);
     // init_server returns 0 when done.  If it has finished, us too.
     // if an error in init_server is reported, we report it here too.
     return init_server(_infd, _outfd);
    }
    abort();
  }
  return 0;
}

//
// Although init_server only returns 0 for now (errors don't interrupt)
// we could return 1 in case of a problem
//
uint64_t
init_server (int fdin, int fdout)
{
  struct syserv_mesg mesg;

  signal_handler_init();

  write_mesg(fdout, SYSERV_READY, 0);

  for (;;) {
    int ret = read_mesg(fdin, &mesg);

    // failure on read from pipe
    if (ret == FAILURE) {
      err(1, "read from fdin failed");
    }

    // exit
    if (ret == END_OF_FILE || mesg.type == SYSERV_EXIT) {
      break;
    }

    // ack
    if (mesg.type == SYSERV_ACK) {
      write_mesg(fdout, SYSERV_ACK, 0);
    }

    // query
    else if (mesg.type == SYSERV_QUERY) {
      write_mesg(fdout, SYSERV_ACK, 0);
      do_query(fdin, fdout, &mesg);
    }

    // unknown message
    else {
      err(1, "unknown mesg type from client: %d", mesg.type);
    }
  }

  //
  // if we've finished, return
  //
  return 0;
//   exit(0);
}


//*****************************************************************
// fnbounds server
//*****************************************************************

void
do_query(int fdin, int fdout, struct syserv_mesg *mesg)
{
  int ret2;

  char* inbuf = malloc(mesg->len);
  if (inbuf == NULL) {
    err(1, "malloc for inbuf failed");
  }

  // read the string following the message
  ret2 = read_all(fdin, inbuf, mesg->len);
  if (ret2 != SUCCESS) {
    err(1, "read from fdin failed");
  }

  TMSG(FNBOUNDS, "begin processing %s -- %s", strrchr(inbuf, '/'), inbuf );
  FnboundsResponse ret = fnb_get_funclist(inbuf);
  send_funcs(fdout, &ret, inbuf);
  if (ret.entries == NULL) {
    // send the error message to the server
    int rets = write_mesg(fdout, SYSERV_ERR, 0);
    if (rets != SUCCESS) {
      errx(1, "Server send error message failed");
    }  // if success, message has been written in send_funcs
  }
  free(ret.entries);
  return;
}



// Send the list of functions to the client
void
send_funcs (int fdout, const FnboundsResponse* response, const char* inbuf)
{
  int ret;

  // send the OK mesg with the count of addresses
  ret = write_mesg(fdout, SYSERV_OK, response->num_entries+1);
  if (ret != SUCCESS) {
    errx(1, "Server write to fdout failed");
  }

  // Write all the values to the stream, plus a zero at the end
  write_all(fdout, response->entries, response->num_entries * sizeof(void*));
  void* zero = 0;
  write_all(fdout, &zero, sizeof(void*));

  // now send the fnb end record
  struct syserv_fnbounds_info fnb_info;
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    fnb_info.memsize = usage.ru_maxrss;
  } else {
    fnb_info.memsize = -1;
  }
  fnb_info.num_entries = response->num_entries;
  fnb_info.is_relocatable = response->is_relocatable;
  fnb_info.reference_offset = response->reference_offset;

  fnb_info.magic = FNBOUNDS_MAGIC;
  fnb_info.status = SYSERV_OK;
  ret = write_all(fdout, &fnb_info, sizeof(fnb_info));
  if (ret != SUCCESS) {
    err(1, "Server fnb_into write_all to fdout failed");
  }
}

//*****************************************************************
// I/O helper functions
//*****************************************************************

// Automatically restart short reads over a pipe.
// Returns: SUCCESS, FAILURE or END_OF_FILE.
//
int
read_all(int fd, void *buf, size_t count)
{
  ssize_t ret;
  size_t len;

  len = 0;
  while (len < count) {
    ret = read(fd, ((char *) buf) + len, count - len);
    if (ret < 0 && errno != EINTR) {
      return FAILURE;
    }
    if (ret == 0) {
      return END_OF_FILE;
    }
    if (ret > 0) {
      len += ret;
    }
  }

  return SUCCESS;
}


// Automatically restart short writes over a pipe.
// Returns: SUCCESS or FAILURE.
//
int
write_all(int fd, const void *buf, size_t count)
{
  ssize_t ret;
  size_t len;

  len = 0;
  while (len < count) {
    ret = write(fd, ((const char *) buf) + len, count - len);
    if (ret < 0 && errno != EINTR) {
      return FAILURE;
    }
    if (ret > 0) {
      len += ret;
    }
  }

  return SUCCESS;
}


// Read a single syserv mesg from incoming pipe.
// Returns: SUCCESS, FAILURE or END_OF_FILE.
//
int
read_mesg(int fdin, struct syserv_mesg *mesg)
{
  int ret;

  memset(mesg, 0, sizeof(*mesg));
  ret = read_all(fdin, mesg, sizeof(*mesg));
  if (ret == SUCCESS && mesg->magic != SYSERV_MAGIC) {
    ret = FAILURE;
  }
  return ret;
}


// Write a single syserv mesg to outgoing pipe.
// Returns: SUCCESS or FAILURE.
//
int
write_mesg(int fdout, int32_t type, int64_t len)
{
  struct syserv_mesg mesg;

  mesg.magic = SYSERV_MAGIC;
  mesg.type = type;
  mesg.len = len;

  return write_all(fdout, &mesg, sizeof(mesg));
}


//*****************************************************************
// signal handlers
//*****************************************************************

void
signal_handler(int sig)
{
  // SIGPIPE means that hpcrun has exited, probably prematurely.
  if (sig == SIGPIPE) {
    errx(0, "hpcrun has prematurely exited");
  }

  // The other signals indicate an internal error.
  errx(1, "got signal outside sigsetjmp: %d", sig);
}


// Catch segfaults, abort and SIGPIPE.
void
signal_handler_init(void)
{
  struct sigaction act;

  act.sa_handler = signal_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);

  if (sigaction(SIGBUS, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGBUS");
  }
  if (sigaction(SIGABRT, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGABRT");
  }
  if (sigaction(SIGPIPE, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGPIPE");
  }
}
