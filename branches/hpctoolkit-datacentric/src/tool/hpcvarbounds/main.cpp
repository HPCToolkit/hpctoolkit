// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcvarbounds/main.cpp $
// $Id: main.cpp 4099 2013-02-10 20:13:32Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <vector>
#include <string>

#include <climits>
#include <cstdio>
#include <cstdlib>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

//*****************************************************************************
// HPCToolkit Externals Include
//*****************************************************************************

#include <libdwarf.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "function-entries.h"
#include "server.h"
#include "syserv-mesg.h"
#include "Symtab.h"
#include "Symbol.h"

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;

//*****************************************************************************
// macros
//*****************************************************************************
#define STRLEN(s) (sizeof(s) - 1)

#define DWARF_OK(e) (DW_DLV_OK == (e))

//*****************************************************************************
// forward declarations
//*****************************************************************************

static void usage(char *command, int status);
static void setup_segv_handler(void);

//*****************************************************************************
// local variables
//*****************************************************************************

// output is text mode unless C or server mode is specified.

enum { MODE_TEXT = 1, MODE_C, MODE_SERVER };
static int the_mode = MODE_TEXT;

static bool verbose = false; // additional verbosity

static jmp_buf segv_recover; // handle longjmp "restart" from segv

//*****************************************************************
// interface operations
//*****************************************************************

// Now write one format (C or text) to stdout, or else binary format
// over a pipe in server mode.  No output directory.

int 
main(int argc, char* argv[])
{
  char *object_file;
  int n, fdin, fdout;

  for (n = 1; n < argc; n++) {
    if (strcmp(argv[n], "-c") == 0) {
      the_mode = MODE_C;
    }
    else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0) {
      usage(argv[0], 0);
    }
    else if (strcmp(argv[n], "-s") == 0) {
      the_mode = MODE_SERVER;
      if (argc < n + 3 || sscanf(argv[n+1], "%d", &fdin) < 1
	  || sscanf(argv[n+2], "%d", &fdout) < 1) {
	fprintf(stderr, "%s: missing file descriptors for server mode\n",
		argv[0]);
	exit(1);
      }
      n += 2;
    }
    else if (strcmp(argv[n], "-t") == 0) {
      the_mode = MODE_TEXT;
    }
    else if (strcmp(argv[n], "-v") == 0) {
      verbose = true;
    }
    else if (strcmp(argv[n], "--") == 0) {
      n++;
      break;
    }
    else if (strncmp(argv[n], "-", 1) == 0) {
      fprintf(stderr, "%s: unknown option: %s\n", argv[0], argv[n]);
      usage(argv[0], 1);
    }
    else {
      break;
    }
  }

  // Run as the system server.
  if (server_mode()) {
    system_server(fdin, fdout);
    exit(0);
  }

  // Must specify at least the object file.
  if (n >= argc) {
    usage(argv[0], 1);
  }
  object_file = argv[n];

  setup_segv_handler();
  if ( ! setjmp(segv_recover) ) {
    dump_file_info(object_file);
  }
  else {
    fprintf(stderr,
	    "!!! INTERNAL hpcvarbounds-bin error !!!\n"
	    "argument string = ");
    for (int i = 0; i < argc; i++)
      fprintf(stderr, "%s ", argv[i]);
    fprintf(stderr, "\n");
  }
  return 0;
}

int
c_mode(void)
{
  return the_mode == MODE_C;
}

int
server_mode(void)
{
  return the_mode == MODE_SERVER;
}


extern "C" {

// we don't care about demangled names. define
// a no-op that meets symtabAPI semantic needs only
char *
cplus_demangle(char *s, int opts)
{
return strdup(s);
}  

};


//*****************************************************************
// private operations
//*****************************************************************

static void 
usage(char *command, int status)
{
  fprintf(stderr, 
    "Usage: hpcvarbounds [options] object-file\n\n"
    "\t-c\twrite output in C source code\n"
    "\t-h\tprint this help message and exit\n"
    "\t-s fdin fdout\trun in server mode\n"
    "\t-t\twrite output in text format (default)\n"
    "\t-v\tturn on verbose output in hpcvarbounds script\n\n"
    "If no format is specified, then text mode is used.\n");

  exit(status);
}

static void
segv_handler(int sig)
{
  longjmp(segv_recover, 1);
}

static void
setup_segv_handler(void)
{
#if 0
  const struct sigaction segv_action= {
    .sa_handler = segv_handler,
    .sa_flags   = 0
  };
#endif
  struct sigaction segv_action;
  segv_action.sa_handler = segv_handler;
  segv_action.sa_flags   = 0;
  sigemptyset(&segv_action.sa_mask);

  sigaction(SIGSEGV, &segv_action, NULL);
}

static void 
dump_symbols(int dwarf_fd, Symtab *syms, vector<Symbol *> &symvec)
{
  for (unsigned int i = 0; i < symvec.size(); i++) {
    Symbol *s = symvec[i];
    if (s->getOffset() != 0) 
      add_variable_entry((void *) s->getOffset(), s->getSize(), &s->getMangledName(), 1);
  }

//  seed_dwarf_info(dwarf_fd);

  //-----------------------------------------------------------------
  // dump the address and comment for each function  
  //-----------------------------------------------------------------
  dump_variables();
}


static void 
dump_file_symbols(int dwarf_fd, Symtab *syms, vector<Symbol *> &symvec)
{
  if (c_mode()) {
    printf("unsigned long hpcrun_nm_addrs[] = {\n");
  }

  dump_symbols(dwarf_fd, syms, symvec);

  if (c_mode()) {
    printf("\n};\n");
  }
}


// We call it "header", even though it comes at end of file.
//
static void
dump_header_info(int is_relocatable, uintptr_t ref_offset)
{
  if (server_mode()) {
    syserv_add_header(is_relocatable, ref_offset);
    return;
  }

  if (c_mode()) {
    printf("unsigned long hpcrun_nm_addrs_len = "
	   "sizeof(hpcrun_nm_addrs) / sizeof(hpcrun_nm_addrs[0]);\n"
	   "unsigned long hpcrun_reference_offset = 0x%" PRIxPTR ";\n"
	   "int hpcrun_is_relocatable = %d;\n",
	   ref_offset, is_relocatable);
    return;
  }

  // default is text mode
  printf("num symbols = %ld, reference offset = 0x%" PRIxPTR ", "
	 "relocatable = %d\n",
	 num_variable_entries(), ref_offset, is_relocatable);
}


static void
assert_file_is_readable(const char *filename)
{
  struct stat sbuf;
  int ret = stat(filename, &sbuf);
  if (ret != 0 || !S_ISREG(sbuf.st_mode)) {
    fprintf(stderr, "hpcvarbounds: unable to open file: %s\n", filename);
    exit(-1);
  } 
}


void 
dump_file_info(const char *filename)
{
  Symtab *syms;
  string sfile(filename);
  vector<Symbol *> symvec;
  uintptr_t image_offset = 0;

  assert_file_is_readable(filename);

  if ( ! Symtab::openFile(syms, sfile) ) {
    fprintf(stderr,
	    "!!! INTERNAL hpcvarbounds-bin error !!!\n"
	    "  -- file %s is readable, but Symtab::openFile fails !\n",
	    filename);
    exit(1);
  }
  int relocatable = 0;

  // open for dwarf usage
  int dwarf_fd = open(filename, O_RDONLY);

  syms->getAllSymbolsByType(symvec, Symbol::ST_OBJECT);

  if (syms->getObjectType() != obj_Unknown) {
    dump_file_symbols(dwarf_fd, syms, symvec);
    relocatable = syms->isExec() ? 0 : 1;
    image_offset = syms->imageOffset();
  }
  dump_header_info(relocatable, image_offset);

  //-----------------------------------------------------------------
  // free as many of the Symtab objects as we can
  //-----------------------------------------------------------------

  close(dwarf_fd);

  Symtab::closeSymtab(syms);
}
