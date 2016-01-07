// -*-Mode: C++;-*-

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

#include "code-ranges.h"
#include "process-ranges.h"
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

#define SECTION_SYMTAB ".symtab"
#define SECTION_INIT   ".init"
#define SECTION_FINI   ".fini"
#define SECTION_TEXT   ".text"
#define SECTION_PLT    ".plt"
#define SECTION_GOT    ".got"

#define PATHSCALE_EXCEPTION_HANDLER_PREFIX "Handler."
#define USE_PATHSCALE_SYMBOL_FILTER
#define USE_SYMTABAPI_EXCEPTION_BLOCKS

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
  DiscoverFnTy fn_discovery = DiscoverFnTy_Aggressive;
  char *object_file;
  int n, fdin, fdout;

  for (n = 1; n < argc; n++) {
    if (strcmp(argv[n], "-c") == 0) {
      the_mode = MODE_C;
    }
    else if (strcmp(argv[n], "-d") == 0) {
      fn_discovery = DiscoverFnTy_Conservative;
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
    system_server(fn_discovery, fdin, fdout);
    exit(0);
  }

  // Must specify at least the object file.
  if (n >= argc) {
    usage(argv[0], 1);
  }
  object_file = argv[n];

  setup_segv_handler();
  if ( ! setjmp(segv_recover) ) {
    dump_file_info(object_file, fn_discovery);
  }
  else {
    fprintf(stderr,
	    "!!! INTERNAL hpcfnbounds-bin error !!!\n"
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
    "Usage: hpcfnbounds [options] object-file\n\n"
    "\t-c\twrite output in C source code\n"
    "\t-d\tdon't perform function discovery on stripped code\n"
    "\t-h\tprint this help message and exit\n"
    "\t-s fdin fdout\trun in server mode\n"
    "\t-t\twrite output in text format (default)\n"
    "\t-v\tturn on verbose output in hpcfnbounds script\n\n"
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


static bool 
matches_prefix(string s, const char *pre, int n)
{
  const char *sc = s.c_str();
  return strncmp(sc, pre, n) == 0;
}

#ifdef __PPC64__
static bool 
matches_contains(string s, const char *substring)
{
  const char *sc = s.c_str();
  return strstr(sc, substring) != 0;
}
#endif

static bool 
pathscale_filter(Symbol *sym)
{
  bool result = false;
  // filter out function symbols for exception handlers
  if (matches_prefix(sym->getMangledName(), 
		     PATHSCALE_EXCEPTION_HANDLER_PREFIX, 
		     STRLEN(PATHSCALE_EXCEPTION_HANDLER_PREFIX))) 
    result = true;
  return result;
}


static bool 
report_symbol(Symbol *sym)
{
#ifdef USE_PATHSCALE_SYMBOL_FILTER
  if (pathscale_filter(sym)) return false;
#endif
  return true;
}


static string * 
code_range_comment(string &name, string section, const char *which)
{
  name = which; 
  name = name + " " + section + " section";
  return &name;
}


static void
note_code_range(Region *s, long memaddr, DiscoverFnTy discover)
{
  char *start = (char *) s->getDiskOffset();
  char *end = start + s->getDiskSize();
  string ntmp;
  new_code_range(start, end, memaddr, discover);

  add_function_entry(start, code_range_comment(ntmp, s->getRegionName(), "start"), true /* global */);
  add_function_entry(end, code_range_comment(ntmp, s->getRegionName(), "end"), true /* global */);
}


static void
note_section(Symtab *syms, const char *sname, DiscoverFnTy discover)
{
  long memaddr = (long) syms->mem_image();
  Region *s;
  if (syms->findRegion(s, sname) && s) 
    note_code_range(s, memaddr - syms->imageOffset(), discover);
}


static void
note_code_ranges(Symtab *syms, DiscoverFnTy fn_discovery)
{
  //TODO: instead of just considering specific segments below
  //      perhaps we should consider all segments marked executable.
  //      binaries could include "bonus" segments we don't
  //      know about explicitly as having code within.
  note_section(syms, SECTION_INIT, fn_discovery);
  note_section(syms, SECTION_PLT,  DiscoverFnTy_Aggressive);
  note_section(syms, SECTION_TEXT, fn_discovery);
  note_section(syms, SECTION_GOT,  DiscoverFnTy_None);
  note_section(syms, SECTION_FINI, fn_discovery);
}

// collect function start addresses from eh_frame info
// (part of the DWARF info)
// enter these start addresses into the reachable function
// data structure

static void
seed_dwarf_info(int dwarf_fd)
{
  Dwarf_Debug dbg = NULL;
  Dwarf_Error err;
  Dwarf_Handler errhand = NULL;
  Dwarf_Ptr errarg = NULL;

  // Unless disabled, add eh_frame info to function entries
  if(getenv("EH_NO")) {
    close(dwarf_fd);
    return;
  }

  if ( ! DWARF_OK(dwarf_init(dwarf_fd, DW_DLC_READ,
                             errhand, errarg,
                             &dbg, &err))) {
    if (verbose) fprintf(stderr, "dwarf init failed !!\n");
    return;
  }

  Dwarf_Cie* cie_data = NULL;
  Dwarf_Signed cie_element_count = 0;
  Dwarf_Fde* fde_data = NULL;
  Dwarf_Signed fde_element_count = 0;

  int fres =
    dwarf_get_fde_list_eh(dbg, &cie_data,
                          &cie_element_count, &fde_data,
                          &fde_element_count, &err);
  if ( ! DWARF_OK(fres)) {
    if (verbose) fprintf(stderr, "failed to get eh_frame element from DWARF\n");
    return;
  }

  for (int i = 0; i < fde_element_count; i++) {
    Dwarf_Addr low_pc = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Ptr fde_bytes = NULL;
    Dwarf_Unsigned fde_bytes_length = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;

    int fres = dwarf_get_fde_range(fde_data[i],
                                   &low_pc, &func_length,
                                   &fde_bytes,
                                   &fde_bytes_length,
                                   &cie_offset, &cie_index,
                                   &fde_offset, &err);
    if (fres == DW_DLV_ERROR) {
      fprintf(stderr, " error on dwarf_get_fde_range\n");
      return;
    }
    if (fres == DW_DLV_NO_ENTRY) {
      fprintf(stderr, " NO_ENTRY error on dwarf_get_fde_range\n");
      return;
    }
    if(getenv("EH_SHOW")) {
      fprintf(stderr, " ---potential fn start = %p\n", reinterpret_cast<void*>(low_pc));
    }

    if (low_pc != (Dwarf_Addr) 0) add_function_entry(reinterpret_cast<void*>(low_pc), NULL, false, 0);
  }
  if ( ! DWARF_OK(dwarf_finish(dbg, &err))) {
    fprintf(stderr, "dwarf finish fails ???\n");
  }
  close(dwarf_fd);
}

static void 
dump_symbols(int dwarf_fd, Symtab *syms, vector<Symbol *> &symvec, DiscoverFnTy fn_discovery)
{
  note_code_ranges(syms, fn_discovery);

  //-----------------------------------------------------------------
  // collect function start addresses and pair them with a comment
  // that indicates what function (or functions) map to that start 
  // address. enter them into a data structure for reachable function
  // processing
  //-----------------------------------------------------------------
  for (unsigned int i = 0; i < symvec.size(); i++) {
    Symbol *s = symvec[i];
    Symbol::SymbolLinkage sl = s->getLinkage();
    if (report_symbol(s) && s->getOffset() != 0) {
      string mname = s->getMangledName();
      add_function_entry((void *) s->getOffset(), &mname,
			 ((sl & Symbol::SL_GLOBAL) ||
			  (sl & Symbol::SL_WEAK)));
    }
  }

  seed_dwarf_info(dwarf_fd);

  process_code_ranges();

  //-----------------------------------------------------------------
  // dump the address and comment for each function  
  //-----------------------------------------------------------------
  dump_reachable_functions();
}


static void 
dump_file_symbols(int dwarf_fd, Symtab *syms, vector<Symbol *> &symvec,
		  DiscoverFnTy fn_discovery)
{
  if (c_mode()) {
    printf("unsigned long hpcrun_nm_addrs[] = {\n");
  }

  dump_symbols(dwarf_fd, syms, symvec, fn_discovery);

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
	 num_function_entries(), ref_offset, is_relocatable);
}


static void
assert_file_is_readable(const char *filename)
{
  struct stat sbuf;
  int ret = stat(filename, &sbuf);
  if (ret != 0 || !S_ISREG(sbuf.st_mode)) {
    fprintf(stderr, "hpcfnbounds: unable to open file: %s\n", filename);
    exit(-1);
  } 
}


void 
dump_file_info(const char *filename, DiscoverFnTy fn_discovery)
{
  Symtab *syms;
  string sfile(filename);
  vector<Symbol *> symvec;
  uintptr_t image_offset = 0;

  assert_file_is_readable(filename);

  if ( ! Symtab::openFile(syms, sfile) ) {
    fprintf(stderr,
	    "!!! INTERNAL hpcfnbounds-bin error !!!\n"
	    "  -- file %s is readable, but Symtab::openFile fails !\n",
	    filename);
    exit(1);
  }
  int relocatable = 0;

  // open for dwarf usage
  int dwarf_fd = open(filename, O_RDONLY);

#ifdef USE_SYMTABAPI_EXCEPTION_BLOCKS 
  //-----------------------------------------------------------------
  // ensure that we don't infer function starts within try blocks or
  // at the start of catch blocks
  //-----------------------------------------------------------------
  vector<ExceptionBlock *> exvec;
  syms->getAllExceptions(exvec);
  
  for (unsigned int i = 0; i < exvec.size(); i++) {
    ExceptionBlock *e = exvec[i];

#ifdef DUMP_EXCEPTION_BLOCK_INFO
    printf("tryStart = %p tryEnd = %p, catchStart = %p\n", e->tryStart(), 
	   e->tryEnd(), e->catchStart()); 
#endif // DUMP_EXCEPTION_BLOCK_INFO
    //-----------------------------------------------------------------
    // prevent inference of function starts within the try block
    //-----------------------------------------------------------------
    add_protected_range((void *) e->tryStart(), (void *) e->tryEnd());
    
    //-----------------------------------------------------------------
    // prevent inference of a function start at the beginning of a
    // catch block. the extent of the catch block is unknown.
    //-----------------------------------------------------------------
    long cs = e->catchStart(); 
    add_protected_range((void *) cs, (void *) (cs + 1));
  }
#endif // USE_SYMTABAPI_EXCEPTION_BLOCKS 

  syms->getAllSymbolsByType(symvec, Symbol::ST_FUNCTION);

#ifdef __PPC64__
  {
    //-----------------------------------------------------------------
    // collect addresses of trampolines for long distance calls as per
    // ppc64 abi. empirically, the linker on BG/Q enters these symbols
    // with the type NOTYPE and a name that contains the substring 
    // "long_branch"
    //-----------------------------------------------------------------
    vector<Symbol *> vec;
    syms->getAllSymbolsByType(vec, Symbol::ST_NOTYPE);
    for (unsigned int i = 0; i < vec.size(); i++) {
      Symbol *s = vec[i];
      string mname = s->getMangledName();
      if (matches_contains(mname, "long_branch") && s->getOffset() != 0) {
	add_function_entry((void *) s->getOffset(), &mname, true);
      }
    }
  }
#endif

  if (syms->getObjectType() != obj_Unknown) {
    dump_file_symbols(dwarf_fd, syms, symvec, fn_discovery);
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
