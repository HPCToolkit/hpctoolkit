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
// Copyright ((c)) 2002-2022, Rice University
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
//

//***************************************************************************

// To build an interactive, stand-alone client for testing:
// (1) turn on this #if and (2) fetch copies of syserv-mesg.h
// and fnbounds_file_header.h.

// Usage:
// To trace the client-side messages, use -v
// To tell the server to run in verbose mode, use -V
// To tell the server not to do agressive function searching, use -d
// To have the client write its output to a file, use -o <outfile>
// The last argument should be the path to the server

#if 0
#define STAND_ALONE_CLIENT
#define EMSG(...)
#define EEMSG(...)
#define TMSG(...)
#define SAMPLE_SOURCES(...)  zero_fcn()
#define dup2(...)  zero_fcn()
#define hpcrun_set_disabled()
#define monitor_real_exit  exit
#define monitor_real_fork  fork
#define monitor_real_execve  execve
#define monitor_sigaction(...)  0
int zero_fcn(void) { return 0; }
int verbose = 0;
int serv_verbose = 0;
int noscan = 0;

#include <stdio.h>
FILE	*outf;

char	*outfile;

#endif

//***************************************************************************

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
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
#include "audit/audit-api.h"
#else
#include "syserv-mesg.h"
#include "fnbounds_file_header.h"
#endif

// Size to allocate for the stack of the server setup function, in KiB.
#define SERVER_STACK_SIZE 1024

#define SUCCESS   0
#define FAILURE  -1
#define END_OF_FILE  -2

enum {
  SYSERV_ACTIVE = 1,
  SYSERV_INACTIVE
};

static int client_status = SYSERV_INACTIVE;
static char *server;
static char *server_stack;

static int fdout = -1;
static int fdin = -1;

static pid_t my_pid;

#if 0
// Limit on memory use at which we restart the server in Meg.
#define SERVER_MEM_LIMIT  140
#define MIN_NUM_QUERIES   12

// rusage units are Kbytes.
static long mem_limit = SERVER_MEM_LIMIT * 1024;
static int  num_queries = 0;
static int  mem_warning = 0;
#endif

extern char **environ;


//*****************************************************************
// Miscellaneous helper function
//*****************************************************************

// Returns: micro-seconds from start to now
static long
tdiff(struct timeval start, struct timeval now)
{
  return 1000000 * (now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec);
}


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

#ifdef STAND_ALONE_CLIENT
  if(verbose) {
    fprintf(stderr, "Client read_all, count = %ld bytes\n", count);
  }
#endif
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

#ifdef STAND_ALONE_CLIENT
  if(verbose) {
    fprintf(stderr, "Client write_all, count = %ld bytes\n", count);
  }
#endif
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
#ifdef STAND_ALONE_CLIENT
  if(verbose) {
    fprintf(stderr, "Client read message, type = %d, len = %ld\n", mesg->type, mesg->len);
  }
#endif

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

#ifdef STAND_ALONE_CLIENT
  if(verbose) {
    fprintf(stderr, "Client write message, type = %d, len = %ld\n", type, len);
  }
#endif
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
  TMSG(FNBOUNDS_CLIENT, "caught SIGPIPE: system server must have exited");
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
  auditor_exports->close(fdout);
  auditor_exports->close(fdin);
  fdout = -1;
  fdin = -1;
  client_status = SYSERV_INACTIVE;

  TMSG(FNBOUNDS_CLIENT, "syserv shutdown");
}

static int
hpcfnbounds_grandchild(void* fds_vp)
{
  struct {
    int sendfd[2], recvfd[2];
  }* fds = fds_vp;

  auditor_exports->close(fds->sendfd[1]);
  auditor_exports->close(fds->recvfd[0]);

  // dup the hpcrun log file fd onto stdout and stderr.
  if (dup2(messages_logfile_fd(), 1) < 0) {
    warn("dup of log fd onto stdout failed");
  }
  if (dup2(messages_logfile_fd(), 2) < 0) {
    warn("dup of log fd onto stderr failed");
  }

  // make the command line and exec
  char *arglist[15];
  char fdin_str[10], fdout_str[10];
  sprintf(fdin_str,  "%d", fds->sendfd[0]);
  sprintf(fdout_str, "%d", fds->recvfd[1]);

  int j = 0;
  arglist[j++] = server;
#ifdef STAND_ALONE_CLIENT
  if (serv_verbose) {
    arglist[j++] = "-v";
  }
  if (noscan) {
    arglist[j++] = "-d";
  }
#else
  // verbose options from hpcrun -dd vars
  if (ENABLED(FNBOUNDS)) {
    arglist[j++] = "-v";
  }
  if (ENABLED(FNBOUNDS_EXT)) {
    arglist[j++] = "-v2";
  }
#endif
  arglist[j++] = "-s";
  arglist[j++] = fdin_str;
  arglist[j++] = fdout_str;
  arglist[j++] = NULL;

  // Exec with the purified environment, so we don't have recursion.
  auditor_exports->execve(server, arglist, auditor_exports->pure_environ);
  err(1, "hpcrun system server: exec(%s) failed", server);
}

static int
hpcfnbounds_child(void* fds_vp) {
  // Clone the grandchild. We can share the memory space here since we're exiting soon.
  errno = 0;
  pid_t grandchild_pid = auditor_exports->clone(hpcfnbounds_grandchild,
    &server_stack[SERVER_STACK_SIZE * 1024], CLONE_UNTRACED | CLONE_VM, fds_vp);
  // If the grandchild clone failed, pass the error back to the parent.
  return grandchild_pid < 0 ? errno != 0 ? errno : -1 : 0;
}

// Returns: 0 on success, else -1 on failure.
static int
launch_server(void)
{
  struct {
    int sendfd[2], recvfd[2];
  } fds;
  bool sampling_is_running = false;
  pid_t child_pid;

  // already running
  if (client_status == SYSERV_ACTIVE && my_pid == getpid()) {
    return 0;
  }

  // new process after fork
  if (client_status == SYSERV_ACTIVE) {
    shutdown_server();
  }

  if (auditor_exports->pipe(fds.sendfd) != 0 || auditor_exports->pipe(fds.recvfd) != 0) {
    EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: pipe failed");
    return -1;
  }

  if (hpcrun_is_initialized()){
    // some sample sources need to be stopped in the parent, or else
    // they cause problems in the child.
    sampling_is_running = SAMPLE_SOURCES(started);
    if (sampling_is_running) {
      SAMPLE_SOURCES(stop);
    }
  }

  // Give up a bit of our stack for the child shim. It doesn't need much.
  char child_stack[4 * 1024 * 2];

  // Clone the child shim. With Glibc <2.24 there is a bug (https://sourceware.org/bugzilla/show_bug.cgi?id=18862)
  // where this will reset the pthreads state in the parent if CLONE_VM is used.
  // Clone the memory space to avoid feedback effects.
  child_pid = auditor_exports->clone(hpcfnbounds_child,
    &child_stack[4 * 1024], CLONE_UNTRACED, &fds);

  if (child_pid < 0) {
    //
    // clone failed
    //
    EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: clone failed");
    return -1;
  }

  // Wait for the child to successfully clone the grandchild
  int status;
  if (auditor_exports->waitpid(child_pid, &status, __WCLONE) < 0) {
    //
    // waitpid failed
    //
    EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: waitpid failed");
    return -1;
  }
  if(!WIFEXITED(status)) {
    //
    // child died mysteriously
    //
    if(WIFSIGNALED(status))
      EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: child shim died by signal %d", WTERMSIG(status));
    else
      EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: child shim died mysteriously");
    return -1;
  }
  if(WEXITSTATUS(status) != 0) {
    //
    // child failed for some other reason
    //
    EMSG("FNBOUNDS_CLIENT ERROR: syserv launch failed: child exited with %d", WEXITSTATUS(status));
    return -1;
  }

  //
  // parent process: return and wait for queries.
  //
  auditor_exports->close(fds.sendfd[0]);
  auditor_exports->close(fds.recvfd[1]);
  fdout = fds.sendfd[1];
  fdin = fds.recvfd[0];
  my_pid = getpid();
  client_status = SYSERV_ACTIVE;

  TMSG(FNBOUNDS_CLIENT, "syserv launch: success, child shim: %d, server: ???", (int) child_pid);

  // Fnbounds talks first with a READY message
  struct syserv_mesg mesg;
  if (read_mesg(&mesg) != SUCCESS) {
    EMSG("FNBOUNDS_CLIENT ERROR: syserv did not give READY message");
    return -1;
  }
  if (mesg.type != SYSERV_READY) {
    EMSG("FNBOUNDS_CLIENT ERROR: syserv gave bad initial message: expected %d, got %d", SYSERV_READY, mesg.type);
    return -1;
  }

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
  server = getenv("HPCRUN_FNBOUNDS_CMD");
  if (server == NULL) {
    EMSG("FNBOUNDS_CLIENT ERROR: unable to get HPCRUN_FNBOUNDS_CMD");
    return -1;
  }

  AMSG("fnbounds: %s", server);

#if 0
  // limit on server memory usage in Meg
  char *str = getenv("HPCRUN_SERVER_MEMSIZE");
  long size;
  if (str == NULL || sscanf(str, "%ld", &size) < 1) {
    size = SERVER_MEM_LIMIT;
  }
  mem_limit = size * 1024;
#endif

  // Allocate enough space for fnbounds summoning.
  // Twice as much to allow for growth in either direction.
  server_stack = mmap_anon(SERVER_STACK_SIZE * 1024 * 2);

  if (monitor_sigaction(SIGPIPE, &hpcrun_sigpipe_handler, 0, NULL) != 0) {
    EMSG("FNBOUNDS_CLIENT ERROR: unable to install handler for SIGPIPE");
  }

  launch_server();

  // check that the server answers ACK
  struct syserv_mesg mesg;
  if (write_mesg(SYSERV_ACK, 0) == SUCCESS && read_mesg(&mesg) == SUCCESS) {
    return 0;
  }

  // if not, then retry one time
  shutdown_server();
  launch_server();
  if (write_mesg(SYSERV_ACK, 0) == SUCCESS && read_mesg(&mesg) == SUCCESS) {
    return 0;
  }

  // if we can't run the fnbounds server, then the profile is useless,
  // so exit.
  shutdown_server();
  EEMSG("hpcrun: unable to launch the hpcfnbounds server.\n"
	"hpcrun: check that hpctoolkit is properly configured with dyninst\n"
	"and its prereqs (boost, elfutils, libdwarf, bzip, libz, lzma).");
  monitor_real_exit(1);

  return -1;
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
  struct timeval start, now;
  struct syserv_mesg mesg;
  void *addr;

  if (fname == NULL || fh == NULL) {
    EMSG("FNBOUNDS_CLIENT ERROR: passed NULL pointer to %s", __func__);
    return NULL;
  }

  if (client_status != SYSERV_ACTIVE || my_pid != getpid()) {
    launch_server();
  }

  TMSG(FNBOUNDS_CLIENT, "query: %s", fname);

  if (ENABLED(FNBOUNDS_CLIENT)) {
    gettimeofday(&start, NULL);
  }

  // Send the file name length (including \0) to the server and look
  // for the initial ACK.  If the server has died, then make one
  // attempt to restart it before giving up.
  //
  size_t len = strlen(fname) + 1;
  if (write_mesg(SYSERV_QUERY, len) != SUCCESS
      || read_mesg(&mesg) != SUCCESS || mesg.type != SYSERV_ACK)
  {
    TMSG(FNBOUNDS_CLIENT, "restart server");
    shutdown_server();
    launch_server();
    if (write_mesg(SYSERV_QUERY, len) != SUCCESS
	|| read_mesg(&mesg) != SUCCESS || mesg.type != SYSERV_ACK)
    {
      EMSG("FNBOUNDS_CLIENT ERROR: unable to restart system server");
      shutdown_server();
      return NULL;
    }
  }

  // Send the file name (including \0) and wait for the initial answer
  // (OK or ERR).  At this point, errors are pretty much fatal.
  //
  if (write_all(fdout, fname, len) != SUCCESS) {
    EMSG("FNBOUNDS_CLIENT ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (read_mesg(&mesg) != SUCCESS) {
    EMSG("FNBOUNDS_CLIENT ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (mesg.type != SYSERV_OK) {
    EMSG("FNBOUNDS_CLIENT ERROR: query failed: %s", fname);
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
    EMSG("FNBOUNDS_CLIENT ERROR: mmap failed");
    shutdown_server();
    return NULL;
  }
  if (read_all(fdin, addr, num_bytes) != SUCCESS) {
    EMSG("FNBOUNDS_CLIENT ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }

  // Read the trailing fnbounds file header.
  struct syserv_fnbounds_info fnb_info;
  int ret = read_all(fdin, &fnb_info, sizeof(fnb_info));
  if (ret != SUCCESS || fnb_info.magic != FNBOUNDS_MAGIC) {
    EMSG("FNBOUNDS_CLIENT ERROR: lost contact with server");
    shutdown_server();
    return NULL;
  }
  if (fnb_info.status != SYSERV_OK) {
    EMSG("FNBOUNDS_CLIENT ERROR: query failed: %s", fname);
    return NULL;
  }
  fh->num_entries = fnb_info.num_entries;
  fh->reference_offset = fnb_info.reference_offset;
  fh->is_relocatable = fnb_info.is_relocatable;
  fh->mmap_size = mmap_size;

  if (ENABLED(FNBOUNDS_CLIENT)) {
    gettimeofday(&now, NULL);
  }

  TMSG(FNBOUNDS_CLIENT, "addr: %p, symbols: %ld, offset: 0x%lx, reloc: %d",
       addr, (long) fh->num_entries, (long) fh->reference_offset,
       (int) fh->is_relocatable);
  TMSG(FNBOUNDS_CLIENT, "server memsize: %ld Meg,  time: %ld usec",
       fnb_info.memsize / 1024, tdiff(start, now));

#if 0
  // Restart the server if it's done a minimum number of queries and
  // has exceeded its memory limit.  Issue a warning at 60%.
  num_queries++;
  if (!mem_warning && fnb_info.memsize > (6 * mem_limit)/10) {
    EMSG("FNBOUNDS_CLIENT: warning: memory usage: %ld Meg",
	 fnb_info.memsize / 1024);
    mem_warning = 1;
  }
  if (num_queries >= MIN_NUM_QUERIES && fnb_info.memsize > mem_limit) {
    EMSG("FNBOUNDS_CLIENT: warning: memory usage: %ld Meg, restart server",
	 fnb_info.memsize / 1024);
    shutdown_server();
  }
#endif

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
    fprintf(outf, "\nclientfnbounds> ");
    if (fgets(fname, BUF_SIZE, stdin) == NULL) {
      break;
    }
    char *new_line = strchr(fname, '\n');
    if (new_line != NULL) {
      *new_line = 0;
    }

    addr = (void **) hpcrun_syserv_query(fname, &fnb_hdr);

    if (addr == NULL) {
      fprintf(outf, "Client error NULL return from hpcrun_syserv_query\n");
    }
    else {
      for (k = 0; k < fnb_hdr.num_entries; k++) {
	fprintf(outf, "  %p\n", addr[k]);
      }
      fprintf(outf, "num symbols = %ld, offset = 0x%lx, reloc = %d\n",
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
  int i;

  outf = stdout;
  server = NULL;
  for (i = 1; i < argc; i++) {
    // fprintf (stderr, "argv[%d] = \"%s\"\n", i, argv[i] );
    if (*argv[i] == '-') {
      // control arguments
      switch (argv[i][1]) {
      case 'd':
        noscan = 1;
        break;
      case 'V':
        serv_verbose = 1;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'o':
        if ( (i+1) >= argc) {
	  errx(1, "outfile must be specified; usage: client [-V} [-v] [-d] [-o outfile] /path/to/fnbounds");
	}
	i++;
	outfile = argv[i];
	outf = fopen(outfile, "w");
	if (outf == NULL) {
	    errx(1,"outfile fopen failed; usage: client [-V} [-v] [-d] [-o outfile] /path/to/fnbounds");
	}
        break;
      default:
	errx(1, "unknown flag; usage: client [-V} [-v] [-d] [-o outfile] /path/to/fnbounds");
	break;
      }
    } else {
      // no - flag; must be the path to the server
      server = argv[i];
      break;	// from argument loop
    }
  }
  // Make sure the server is non-NULL
  if ( (server == NULL) || (strlen(server) == 0) ) {
    errx(1,"NULL server name; usage: client [-V} [-v] [-d] [-o outfile] /path/to/fnbounds");
  }

  memset(&act, 0, sizeof(act));
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGPIPE, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGPIPE");
  }

  if (launch_server() != 0) {
    errx(1, "fnbounds server failed");
  }
  fprintf(outf, "server: %s\n", server);
  fprintf(outf, "parent: %d, child: ???\n", my_pid);
  fprintf(outf, "connected\n");

  query_loop();

  write_mesg(SYSERV_EXIT, 0);

  fprintf(outf, "done\n");
  return 0;
}

#endif  // STAND_ALONE_CLIENT
