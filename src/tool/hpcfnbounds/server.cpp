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
// system(), use monitor_real_system() for that.
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
// Todo:
// 1. The memory leak is fixed in symtab 8.0.

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

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"
#include "server.h"
#include "syserv-mesg.h"

#define ADDR_SIZE   (256 * 1024)
#define INIT_INBUF_SIZE    2000

#define SUCCESS   0
#define FAILURE  -1
#define END_OF_FILE  -2

static int fdin;
static int fdout;

static void *addr_buf[ADDR_SIZE];
static long  num_addrs;
static long  total_num_addrs;
static long  max_num_addrs;

static char *inbuf;
static long  inbuf_size;

static struct syserv_fnbounds_info fnb_info;

static int jmpbuf_ok = 0;
static sigjmp_buf jmpbuf;

static int sent_ok_mesg;


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
// callback functions
//*****************************************************************

// Called from dump_function_entry().
void
syserv_add_addr(void *addr, long func_entry_map_size)
{
  int ret;

  // send the OK mesg on first addr callback
  if (! sent_ok_mesg) {
    max_num_addrs = func_entry_map_size + 1;
    ret = write_mesg(SYSERV_OK, max_num_addrs);
    if (ret != SUCCESS) {
      errx(1, "write to fdout failed");
    }
    sent_ok_mesg = 1;
  }

  // see if buffer needs to be flushed
  if (num_addrs >= ADDR_SIZE) {
    ret = write_all(fdout, addr_buf, num_addrs * sizeof(void *));
    if (ret != SUCCESS) {
      errx(1, "write to fdout failed");
    }
    num_addrs = 0;
  }

  addr_buf[num_addrs] = addr;
  num_addrs++;
  total_num_addrs++;
}


// Called from dump_header_info().
void
syserv_add_header(int is_relocatable, uintptr_t ref_offset)
{
  fnb_info.is_relocatable = is_relocatable;
  fnb_info.reference_offset = ref_offset;
}


//*****************************************************************
// signal handlers
//*****************************************************************

static void
signal_handler(int sig)
{
  // SIGPIPE means that hpcrun has exited, probably prematurely.
  if (sig == SIGPIPE) {
    errx(0, "hpcrun has prematurely exited");
  }

  // The other signals indicate an internal error.
  if (jmpbuf_ok) {
    siglongjmp(jmpbuf, 1);
  }
  errx(1, "got signal outside sigsetjmp: %d", sig);
}


// Catch segfaults, abort and SIGPIPE.
static void
signal_handler_init(void)
{
  struct sigaction act;

  act.sa_handler = signal_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);

  if (sigaction(SIGSEGV, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGSEGV");
  }
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


//*****************************************************************
// system server
//*****************************************************************

static void
do_query(DiscoverFnTy fn_discovery, struct syserv_mesg *mesg)
{
  int ret;
  long k;

  if (mesg->len > inbuf_size) {
    inbuf_size += mesg->len;
    inbuf = (char *) realloc(inbuf, inbuf_size);
    if (inbuf == NULL) {
      err(1, "realloc for inbuf failed");
    }
  }

  ret = read_all(fdin, inbuf, mesg->len);
  if (ret != SUCCESS) {
    err(1, "read from fdin failed");
  }

  num_addrs = 0;
  total_num_addrs = 0;
  max_num_addrs = 0;
  sent_ok_mesg = 0;

  if (sigsetjmp(jmpbuf, 1) == 0) {
    // initial return on success
    jmpbuf_ok = 1;
    memset(&fnb_info, 0, sizeof(fnb_info));
    code_ranges_reinit();
    function_entries_reinit();

    dump_file_info(inbuf, fn_discovery);
    jmpbuf_ok = 0;

    // pad list of addrs in case there are fewer function addrs than
    // size of map.
    fnb_info.num_entries = total_num_addrs;
    for (k = total_num_addrs; k < max_num_addrs; k++) {
      syserv_add_addr(NULL, 0);
    }
    if (num_addrs > 0) {
      ret = write_all(fdout, addr_buf, num_addrs * sizeof(void *));
      if (ret != SUCCESS) {
	errx(1, "write to fdout failed");
      }
      num_addrs = 0;
    }

    // add rusage maxrss to allow client to track memory usage.
    // units are Kbytes
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
      fnb_info.memsize = usage.ru_maxrss;
    } else {
      fnb_info.memsize = -1;
    }

    fnb_info.magic = FNBOUNDS_MAGIC;
    fnb_info.status = SYSERV_OK;
    ret = write_all(fdout, &fnb_info, sizeof(fnb_info));
    if (ret != SUCCESS) {
      err(1, "write to fdout failed");
    }
  }
  else if (sent_ok_mesg) {
    // failed return from long jmp after we've told the client ok.
    // for now, close the pipe and exit.
    errx(1, "caught signal after telling client ok");
  }
  else {
    // failed return from long jmp before we've told the client ok.
    // in this case, we can send an ERR mesg.
    ret = write_mesg(SYSERV_ERR, 0);
    if (ret != SUCCESS) {
      errx(1, "write to fdout failed");
    }
  }
}


void
system_server(DiscoverFnTy fn_discovery, int fd1, int fd2)
{
  struct syserv_mesg mesg;

  fdin = fd1;
  fdout = fd2;

  inbuf_size = INIT_INBUF_SIZE;
  inbuf = (char *) malloc(inbuf_size);
  if (inbuf == NULL) {
    err(1, "malloc for inbuf failed");
  }
  signal_handler_init();

  for (;;) {
    int ret = read_mesg(&mesg);

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
      write_mesg(SYSERV_ACK, 0);
    }

    // query
    else if (mesg.type == SYSERV_QUERY) {
      write_mesg(SYSERV_ACK, 0);
      do_query(fn_discovery, &mesg);
    }

    // unknown message
    else {
      err(1, "unknown mesg type from client: %d", mesg.type);
    }
  }

  exit(0);
}
