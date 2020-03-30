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
// Copyright ((c)) 2002-2020, Rice University
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

static uint64_t  addr_buf[ADDR_SIZE];
static long  num_addrs;
static long  total_num_addrs;

static char *inbuf;
static long  inbuf_size;

static struct syserv_fnbounds_info fnb_info;

static int jmpbuf_ok = 0;
static sigjmp_buf jmpbuf;

// 
// Although init_server only returns 0 for now (errors don't interrupt)
// we could return 1 in case of a problem
//
uint64_t 
init_server (DiscoverFnTy fn_discovery, int fd1, int fd2)
{
  struct syserv_mesg mesg;

  fdin = fd1;
  fdout = fd2;

// write version into output (.log file in the measurements directory)
  fprintf(stderr, "Begin hpcfnbounds2 server, DiscoverFnTy = %d\n", fn_discovery);

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
do_query(DiscoverFnTy fn_discovery, struct syserv_mesg *mesg)
{
  char *ret;
  int ret2;
  // Make sure the buffer is big enough to hold the name string
  if (mesg->len > inbuf_size) {
    inbuf_size += mesg->len;
    inbuf = (char *) realloc(inbuf, inbuf_size);
    if (inbuf == NULL) {
      err(1, "realloc for inbuf failed");
    }
  }

  // read the string following the message
  ret2 = read_all(fdin, inbuf, mesg->len);
  if (ret2 != SUCCESS) {
    err(1, "read from fdin failed");
  }

  fprintf(stderr, "newfnb begin processing %s -- %s\n", strrchr(inbuf, '/'), inbuf );
  ret = get_funclist(inbuf);
  if ( ret != NULL) {
    fprintf(stderr, "\nServer failure processing %s: %s\n", inbuf, ret );

    // send the error message to the server
    int rets = write_mesg(SYSERV_ERR, 0);
    if (rets != SUCCESS) {
      errx(1, "Server send error message failed");
    }  // if success, message has been written in send_funcs
  }
  return;
}



// Send the list of functions to the client
void
send_funcs ()
{
  int ret;
  int i;

  // count the number of unique addresses to send
  int np = 0;
  uint64_t lastaddr = (uint64_t) -1;
  for (i=0; i<nfunc; i ++) {
    if (farray[i].fadd != lastaddr ){
      np ++;
      lastaddr = farray[i].fadd;
    }
  }
  fprintf(stderr, "newfnb %s = %d (%ld) -- %s\n", strrchr(inbuf, '/'), np, (uint64_t)nfunc, inbuf );

  // send the OK mesg with the count of addresses
  ret = write_mesg(SYSERV_OK, np+1);
  if (ret != SUCCESS) {
    errx(1, "Server write to fdout failed");
  }

  // Now go through the list, and add each unique address
  // see if buffer needs to be flushed
  lastaddr = (uint64_t) -1;
  num_addrs = 0;

  for (i=0; i<nfunc; i ++) {
    if (farray[i].fadd == lastaddr ){
      continue;
    }
    lastaddr = farray[i].fadd;
    if (num_addrs >= ADDR_SIZE) {
      ret = write_all(fdout, addr_buf, num_addrs * sizeof(void *));
      if (ret != SUCCESS) {
        errx(1, "Server write_all to fdout failed");
      } else {
        if (verbose) {
          fprintf(stderr, "Server write_all %ld\n", num_addrs * sizeof(void *) );
	}
      }
      num_addrs = 0;
    }

    addr_buf[num_addrs] = farray[i].fadd;
    num_addrs++;
    total_num_addrs++;
  }
  // we've done the full list, check for buffer filled by the last entry
  if (num_addrs >= ADDR_SIZE) {
    ret = write_all(fdout, addr_buf, num_addrs * sizeof(void *));
    if (ret != SUCCESS) {
      errx(1, "Server write_all to fdout failed");
    } else {
      if (verbose) {
        fprintf(stderr, "Server write_all %ld\n", num_addrs * sizeof(void *) );
      }
    }
    num_addrs = 0;
  }

  // add a zero at the end
  addr_buf[num_addrs] = (uint64_t) 0;
  num_addrs ++;

  // flush the buffer
  ret = write_all(fdout, addr_buf, num_addrs * sizeof(void *));
  if (ret != SUCCESS) {
    errx(1, "Server flush write_all to fdout failed");
  } else {
    if (verbose) {
      fprintf(stderr, "Server flush write_all %ld bytes\n", num_addrs * sizeof(void *) );
    }
  }

  // now send the fnb end record
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    fnb_info.memsize = usage.ru_maxrss;
    fnb_info.memsize = -1;	// Set to -1 to avoid hpcrun killing server
  } else {
    fnb_info.memsize = -1;
  }
  fnb_info.num_entries = np;
  fnb_info.is_relocatable = is_dotso;
  fnb_info.reference_offset = refOffset;

  fnb_info.magic = FNBOUNDS_MAGIC;
  fnb_info.status = SYSERV_OK;
  ret = write_all(fdout, &fnb_info, sizeof(fnb_info));
  if (ret != SUCCESS) {
    err(1, "Server fnb_into write_all to fdout failed");
  } else {
    if (verbose) {
      fprintf(stderr, "Server fnb_info write_all %ld bytes\n", sizeof(fnb_info) );
    }
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
read_mesg(struct syserv_mesg *mesg)
{
  int ret;

  memset(mesg, 0, sizeof(*mesg));
  ret = read_all(fdin, mesg, sizeof(*mesg));
  if (ret == SUCCESS && mesg->magic != SYSERV_MAGIC) {
    ret = FAILURE;
  }
  if (verbose) {
	fprintf(stderr, "Server read  message, type = %d, len = %ld\n",
	    mesg->type, mesg->len);
  }

  return ret;
}


// Write a single syserv mesg to outgoing pipe.
// Returns: SUCCESS or FAILURE.
//
int
write_mesg(int32_t type, int64_t len)
{
  struct syserv_mesg mesg;

  mesg.magic = SYSERV_MAGIC;
  mesg.type = type;
  mesg.len = len;

  if (verbose) {
	fprintf(stderr, "Server write  message, type = %d, len = %ld\n",
	    type, len);
  }
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
  if (jmpbuf_ok) {
    siglongjmp(jmpbuf, 1);
  }
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

#if 0
  if (sigaction(SIGSEGV, &act, NULL) != 0) {
    err(1, "sigaction failed on SIGSEGV");
  }
#endif
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

