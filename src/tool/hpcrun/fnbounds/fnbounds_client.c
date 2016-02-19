// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

// The client side of the new fnbounds server.  Hpcrun creates a pipe
// and then forks and execs hpcfnbounds in server mode (-s).  The file
// descriptors are passed as command-line arguments.
//
// This file implements the client side of the pipe.  For each query,
// send the file name to the server.  On success, mmap space for the
// answer, read the array of addresses from the pipe and read the
// extra info in the fnbounds file header.  The file 'syserv-mesg.h'
// defines the API for messages over the pipe.
//
// Notes:
// 1. Automatically restart the server if it fails.
//
// 2. Automatically restart short reads.  Reading from a pipe can
// return a short answer, especially if the other side buffers the
// writes.
//
// 3. Catch SIGPIPE.  Writing to a pipe after the other side has
// exited triggers a SIGPIPE and terminates the process.  It's
// important to catch (or ignore) SIGPIPE in the client so that if the
// server crashes, it doesn't also kill hpcrun.
//
// 4. We don't need to lock queries to the server.  Calls to
// hpcrun_syserv_query() already hold the FNBOUNDS_LOCK.
//
// 5. Dup the hpcrun log file fd onto stdout and stderr to prevent
// stray output from the server.
//
// 6. The bottom of this file has code for an interactive, stand-alone
// client for testing hpcfnbounds in server mode.
//
// Todo:
// 1. The memory leak is fixed in symtab 8.0.
//
// 2. Kill Zombies!  If the server exits, it will persist as a zombie.
// That's mostly harmless, but we could clean them up with waitpid().
// But we need to do it non-blocking.

//***************************************************************************

// To build an interactive, stand-alone client for testing:
// (1) turn on this #if and (2) fetch copies of syserv-mesg.h
// and fnbounds_file_header.h.

#if 0
#define STAND_ALONE_CLIENT
#define EMSG(...)
#define TMSG(...)
#define SAMPLE_SOURCES(...)  zero_fcn()
#define dup2(...)  zero_fcn()
#define hpcrun_set_disabled()
#define monitor_real_fork  fork
#define monitor_real_execve  execve
#define monitor_sigaction(...)  0
int zero_fcn(void) { return 0; }
#endif

//***************************************************************************

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(STAND_ALONE_CLIENT)
#include <hpcfnbounds/syserv-mesg.h>
#include "client.h"
#include "disabled.h"
#include "fnbounds_file_header.h"
#include "messages.h"
#include "sample_sources_all.h"
#include "monitor.h"
#else
#include "syserv-mesg.h"
#include "fnbounds_file_header.h"
#endif

// Limit on memory use at which we restart the server in Meg.
#define SERVER_MEM_LIMIT  80
#define MIN_NUM_QUERIES   12

#define SUCCESS   0
#define FAILURE  -1
#define END_OF_FILE  -2

enum {
  SYSERV_ACTIVE = 1,
  SYSERV_INACTIVE
};

static int client_status = SYSERV_INACTIVE;
static char *server;

static int fdout = -1;
static int fdin = -1;

static pid_t my_pid;
static pid_t child_pid;

// rusage units are Kbytes.
static long mem_limit = SERVER_MEM_LIMIT * 1024;
static int  num_queries = 0;
static int  mem_warning = 0;

extern char **environ;


//*****************************************************************
// I/O helper functions
//*****************************************************************

// Automatically restart short reads over a pipe.
// Returns: SUCCESS, FAILURE or END_OF_FILE.
//
static int
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
static int
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
static int
read_mesg(struct syserv_mesg *mesg)
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
static int
write_mesg(int32_t type, int64_t len)
{
  struct syserv_mesg mesg;

  mesg.magic = SYSERV_MAGIC;
  mesg.type = type;
  mesg.len = len;

  return write_all(fdout, &mesg, sizeof(mesg));
}


//*****************************************************************
// Mmap Helper Functions
//*****************************************************************

// Returns: 'size' rounded up to a multiple of the mmap page size.
static size_t
page_align(size_t size)
{
  static size_t pagesize = 0;

  if (pagesize == 0) {
#if defined(_SC_PAGESIZE)
    long ans = sysconf(_SC_PAGESIZE);
    if (ans > 0) {
      pagesize = ans;
    }
#endif
    if (pagesize == 0) {
      pagesize = 4096;
    }
  }

  return ((size + pagesize - 1)/pagesize) * pagesize;
}


// Returns: address of anonymous mmap() region, else MAP_FAILED on
// failure.
static void *
mmap_anon(size_t size)
{
  int flags, prot;

  size = page_align(size);
  prot = PROT_READ | PROT_WRITE;
#if defined(MAP_ANONYMOUS)
  flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
  flags = MAP_PRIVATE | MAP_ANON;
#endif

  return mmap(NULL, size, prot, flags, -1, 0);
}


//*****************************************************************
// Signal Handler
//*****************************************************************

// Catch and ignore SIGPIPE so that if the server crashes it doesn't
// also kill hpcrun.
//
static int
hpcrun_sigpipe_handler(int sig, siginfo_t *info, void *context)
{
  TMSG(SYSTEM_SERVER, "caught SIGPIPE: system server must have exited");
  return 0;
}


//*****************************************************************
// Start and Stop the System Server
//*****************************************************************

// Launch the server lazily.

// Close our file descriptors to the server.  If the server is still
// alive, it will detect end-of-file on read() from the pipe.
//
static void
shutdown_server(void)
{
  close(fdout);
  close(fdin);
  fdout = -1;
  fdin = -1;
  client_status = SYSERV_INACTIVE;

  TMSG(SYSTEM_SERVER, "syserv shutdown");
}


// Returns: 0 on success, else -1 on failure.
static int
launch_server(void)
{
  int sendfd[2], recvfd[2];
  bool sampling_is_running;
  pid_t pid;

  // already running
  if (client_status == SYSERV_ACTIVE && my_pid == getpid()) {
    return 0;
  }

  // new process after fork
  if (client_status == SYSERV_ACTIVE) {
    shutdown_server();
  }

  if (pipe(sendfd) != 0 || pipe(recvfd) != 0) {
    EMSG("SYSTEM_SERVER ERROR: syserv launch failed: pipe failed");
    return -1;
  }

  // some sample sources need to be stopped in the parent, or else
  // they cause problems in the child.
  sampling_is_running = SAMPLE_SOURCES(started);
  if (sampling_is_running) {
    SAMPLE_SOURCES(stop);
  }
  pid = monitor_real_fork();

  if (pid < 0) {
    //
    // fork failed
    //
    EMSG("SYSTEM_SERVER ERROR: syserv launch failed: fork failed");
    return -1;
  }
  else if (pid == 0) {
    //
    // child process: disable profiling, dup the log file fd onto
    // stderr and exec hpcfnbounds in server mode.
    //
    hpcrun_set_disabled();

    close(sendfd[1]);
    close(recvfd[0]);

    // dup the hpcrun log file fd onto stdout and stderr.
    if (dup2(messages_logfile_fd(), 1) < 0) {
      warn("dup of log fd onto stdout failed");
    }
    if (dup2(messages_logfile_fd(), 2) < 0) {
      warn("dup of log fd onto stderr failed");
    }

    // make the command line and exec
    char *arglist[8];
    char fdin_str[10], fdout_str[10];
    sprintf(fdin_str,  "%d", sendfd[0]);
    sprintf(fdout_str, "%d", recvfd[1]);

    arglist[0] = server;
    arglist[1] = "-s";
    arglist[2] = fdin_str;
    arglist[3] = fdout_str;
    arglist[4] = NULL;

    monitor_real_execve(server, arglist, environ);
    err(1, "hpcrun system server: exec(%s) failed", server);
  }

  //
  // parent process: return and wait for queries.
  //
  close(sendfd[0]);
  close(recvfd[1]);
  fdout = sendfd[1];
  fdin = recvfd[0];
  my_pid = getpid();
  child_pid = pid;
  client_status = SYSERV_ACTIVE;
  num_queries = 0;
  mem_warning = 0;

  TMSG(SYSTEM_SERVER, "syserv launch: success, server: %d", (int) child_pid);

  // restart sample sources
  if (sampling_is_running) {
    SAMPLE_SOURCES(start);
  }

  return 0;
}


// Returns: 0 on success, else -1 on failure.
int
hpcrun_syserv_init(void)
{
  TMSG(SYSTEM_SERVER, "syserv init");

  server = getenv("HPCRUN_FNBOUNDS_CMD");
  if (server == NULL) {
    EMSG("SYSTEM_SERVER ERROR: unable to get HPCRUN_FNBOUNDS_CMD");
    return -1;
  }

  // limit on server memory usage in Meg
  char *str = getenv("HPCRUN_SERVER_MEMSIZE");
  long size;
  if (str == NULL || sscanf(str, "%ld", &size) < 1) {
    size = SERVER_MEM_LIMIT;
  }
  mem_limit = size * 1024;

  if (monitor_sigaction(SIGPIPE, &hpcrun_sigpipe_handler, 0, NULL) != 0) {
    EMSG("SYSTEM_SERVER ERROR: unable to install handler for SIGPIPE");
  }

  return launch_server();
}


void
hpcrun_syserv_fini(void)
{
  // don't tell the server to exit unless we're the process that
  // started it.
  if (client_status == SYSERV_ACTIVE && my_pid == getpid()) {
    write_mesg(SYSERV_EXIT, 0);
  }
  shutdown_server();

  TMSG(SYSTEM_SERVER, "syserv fini");
}


//*****************************************************************
// Query the System Server
//*****************************************************************

// Returns: pointer to array of void * and fills in the file header,
// or else NULL on error.
//
void *
hpcrun_syserv_query(const char *fname, struct fnbounds_file_header *fh)
{
  struct syserv_mesg mesg;
  void *addr;

  if (fname == NULL || fh == NULL) {
    EMSG("SYSTEM_SERVER ERROR: passed NULL pointer to %s", __func__);
    return NULL;
  }

  if (client_status != SYSERV_ACTIVE || my_pid != getpid()) {
    launch_server();
  }

  TMSG(SYSTEM_SERVER, "query: %s", fname);

  // Send the file name length (including \0) to the server and look
  // for the initial ACK.  If the server has died, then make one
  // attempt to restart it before giving up.
  //
  size_t len = strlen(fname) + 1;
  if (write_mesg(SYSERV_QUERY, len) != SUCCESS
      || read_mesg(&mesg) != SUCCESS || mesg.type != SYSERV_ACK)
  {
    TMSG(SYSTEM_SERVER, "restart server");
    shutdown_server();
    launch_server();
    if (write_mesg(SYSERV_QUERY, len) != SUCCESS
	|| read_mesg(&mesg) != SUCCESS || mesg.type != SYSERV_ACK)
    {
      EMSG("SYSTEM_SERVER ERROR: unable to restart system server");
      shutdown_server();
      return NULL;
    }
  }

  // Send the file name (including \0) and wait for the initial answer
  // (OK or ERR).  At this point, errors are pretty much fatal.
  //
  if (write_all(fdout, fname, len) != SUCCESS) {
    EMSG("SYSTEM_SERVER ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (read_mesg(&mesg) != SUCCESS) {
    EMSG("SYSTEM_SERVER ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (mesg.type != SYSERV_OK) {
    EMSG("SYSTEM_SERVER ERROR: query failed: %s", fname);
    return NULL;
  }

  // Mmap a region for the answer and read the array of addresses.
  // Note: mesg.len is the number of addrs, not bytes.
  //
  size_t num_bytes = mesg.len * sizeof(void *);
  size_t mmap_size = page_align(num_bytes);
  addr = mmap_anon(mmap_size);
  if (addr == MAP_FAILED) {
    // Technically, we could keep the server alive in this case.
    // But we would have to read all the data to stay in sync with
    // the server.
    EMSG("SYSTEM_SERVER ERROR: mmap failed");
    shutdown_server();
    return NULL;
  }
  if (read_all(fdin, addr, num_bytes) != SUCCESS) {
    EMSG("SYSTEM_SERVER ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }

  // Read the trailing fnbounds file header.
  struct syserv_fnbounds_info fnb_info;
  int ret = read_all(fdin, &fnb_info, sizeof(fnb_info));
  if (ret != SUCCESS || fnb_info.magic != FNBOUNDS_MAGIC) {
    EMSG("SYSTEM_SERVER ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (fnb_info.status != SYSERV_OK) {
    EMSG("SYSTEM_SERVER ERROR: query failed: %s", fname);
    return NULL;
  }
  fh->num_entries = fnb_info.num_entries;
  fh->reference_offset = fnb_info.reference_offset;
  fh->is_relocatable = fnb_info.is_relocatable;
  fh->mmap_size = mmap_size;

  TMSG(SYSTEM_SERVER, "addr: %p, symbols: %ld, offset: 0x%lx, reloc: %d",
       addr, (long) fh->num_entries, (long) fh->reference_offset,
       (int) fh->is_relocatable);
  TMSG(SYSTEM_SERVER, "server memsize: %ld Meg", fnb_info.memsize / 1024);

  // Restart the server if it's done a minimum number of queries and
  // has exceeded its memory limit.  Issue a warning at 60%.
  num_queries++;
  if (!mem_warning && fnb_info.memsize > (6 * mem_limit)/10) {
    EMSG("SYSTEM_SERVER: warning: memory usage: %ld Meg",
	 fnb_info.memsize / 1024);
    mem_warning = 1;
  }
  if (num_queries >= MIN_NUM_QUERIES && fnb_info.memsize > mem_limit) {
    EMSG("SYSTEM_SERVER: warning: memory usage: %ld Meg, restart server",
	 fnb_info.memsize / 1024);
    shutdown_server();
  }

  return addr;
}


//*****************************************************************
// Stand Alone Client
//*****************************************************************

#ifdef STAND_ALONE_CLIENT

#define BUF_SIZE  2000

static void
query_loop(void)
{
  struct fnbounds_file_header fnb_hdr;
  char fname[BUF_SIZE];
  void **addr;
  long k;

  for (;;) {
    printf("\nfnbounds> ");
    if (fgets(fname, BUF_SIZE, stdin) == NULL) {
      break;
    }
    char *new_line = strchr(fname, '\n');
    if (new_line != NULL) {
      *new_line = 0;
    }

    addr = (void **) hpcrun_syserv_query(fname, &fnb_hdr);

    if (addr == NULL) {
      printf("error\n");
    }
    else {
      for (k = 0; k < fnb_hdr.num_entries; k++) {
	printf("  %p\n", addr[k]);
      }
      printf("num symbols = %ld, offset = 0x%lx, reloc = %d\n",
	     fnb_hdr.num_entries, fnb_hdr.reference_offset,
	     fnb_hdr.is_relocatable);

      if (munmap(addr, fnb_hdr.mmap_size) != 0) {
	err(1, "munmap failed");
      }
    }
  }
}


int
main(int argc, char *argv[])
{
  struct sigaction act;

  if (argc < 2) {
    errx(1, "usage: client /path/to/fnbounds");
  }
  server = argv[1];

  memset(&act, 0, sizeof(act));
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGPIPE, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGPIPE");
  }

  if (launch_server() != 0) {
    errx(1, "fnbounds server failed");
  }
  printf("server: %s\n", server);
  printf("parent: %d, child: %d\n", my_pid, child_pid);
  printf("connected\n");

  query_loop();

  write_mesg(SYSERV_EXIT, 0);

  printf("done\n");
  return 0;
}

#endif  // STAND_ALONE_CLIENT
