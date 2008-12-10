//
// $Id$
//
//*****************************************************************************
// system includes
//*****************************************************************************

#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "code-ranges.h"
#include "process-ranges.h"
#include "function-entries.h"
#include "fnbounds-file-header.h"
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

#define PATHSCALE_EXCEPTION_HANDLER_PREFIX "Handler."
#define USE_PATHSCALE_SYMBOL_FILTER

#define STRLEN(s) (sizeof(s) - 1)



//*****************************************************************************
// forward declarations
//*****************************************************************************

static void usage(char *command, int status);
static void dump_file_info(const char *filename, bool fn_discovery);


//*****************************************************************************
// local variables
//*****************************************************************************

// We write() the binary format to a file descriptor and fprintf() the
// text and classic C formats to a FILE pointer.
//
static int   the_binary_fd = -1;
static FILE *the_c_fp = NULL;
static FILE *the_text_fp = NULL;


//*****************************************************************
// interface operations
//*****************************************************************

// Parse the options to determine which output formats to write, and
// open the files or set to stdout.  We allow multiple formats, but
// only one to stdout.
//
int 
main(int argc, char **argv)
{
  bool fn_discovery = 1;
  char buf[PATH_MAX], *object_file, *output_dir, *base;
  int num_fmts = 0, do_binary = 0, do_c = 0, do_text = 0;
  int n;

  for (n = 1; n < argc; n++) {
    if (strcmp(argv[n], "-b") == 0) {
      do_binary = 1;
    }
    else if (strcmp(argv[n], "-c") == 0) {
      do_c = 1;
    }
    else if (strcmp(argv[n], "-d") == 0) {
      fn_discovery = 0;
    }
    else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0) {
      usage(argv[0], 0);
    }
    else if (strcmp(argv[n], "-t") == 0) {
      do_text = 1;
    }
    else if (strcmp(argv[n], "-v") == 0) {
      // Ignored at this layer, used in hpcfnbounds script.
    }
    else
      break;
  }
  num_fmts = do_binary + do_c + do_text;

  // Must specify at least the object file.
  if (n >= argc) {
    usage(argv[0], 1);
  }
  object_file = argv[n];
  base = rindex(object_file, '/');
  base = (base == NULL) ? object_file : base + 1;

  // If don't specify the output directory, then limited to one output
  // format and stdout.
  if (n + 1 >= argc && num_fmts > 1) {
    fprintf(stderr,
      "Error: when writing two or more output formats at the same time,\n"
      "you must specify an output directory.\n\n");
    usage(argv[0], 1);
  }
  output_dir = (n + 1 < argc) ? argv[n+1] : NULL;

  // If no output formats given, then use text.
  if (num_fmts == 0)
    do_text = 1;

  // For each output format, open the file or set to stdout.
  if (output_dir != NULL) {
    if (do_binary) {
      sprintf(buf, FNBOUNDS_BINARY_FORMAT, output_dir, base);
      the_binary_fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (the_binary_fd < 0)
        err(1, "open failed on: %s", buf);
    }
    if (do_c) {
      sprintf(buf, FNBOUNDS_C_FORMAT, output_dir, base);
      the_c_fp = fopen(buf, "w");
      if (the_c_fp == NULL)
        err(1, "open failed on: %s", buf);
    }
    if (do_text) {
      sprintf(buf, FNBOUNDS_TEXT_FORMAT, output_dir, base);
      the_text_fp = fopen(buf, "w");
      if (the_text_fp == NULL)
        err(1, "open failed on: %s", buf);
    }
  } else {
    // At most one format when writing to stdout.
    if (do_binary)
      the_binary_fd = 1;
    else if (do_c)
      the_c_fp = stdout;
    else
      the_text_fp = stdout;
  }

  dump_file_info(object_file, fn_discovery);
  return 0;
}


int
binary_fmt_fd(void)
{
  return (the_binary_fd);
}


FILE *
c_fmt_fp(void)
{
  return (the_c_fp);
}


FILE *
text_fmt_fp(void)
{
  return (the_text_fp);
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
    "Usage: hpcfnbounds [options] object-file [output-directory]\n\n"
    "\t-b\twrite output in binary format\n"
    "\t-c\twrite output in C source code\n"
    "\t-d\tdon't perform function discovery on stripped code\n"
    "\t-h\tprint this help message and exit\n"
    "\t-t\twrite output in text format\n"
    "\t-v\tturn on verbose output in hpcfnbounds script\n\n"
    "Multiple output formats (-b, -c, -t) may be specified in one run.\n"
    "If output-directory is not specified, then output is written to\n"
    "stdout and only one format may be used.\n"
    "If no format is specified, then text mode is used.\n");

  exit(status);
}


static bool 
matches_prefix(string s, const char *pre, int n)
{
  const char *sc = s.c_str();
  return strncmp(sc, pre, n) == 0;
}


static bool 
pathscale_filter(Symbol *sym)
{
  bool result = false;
  // filter out function symbols for exception handlers
  if (matches_prefix(sym->getName(), 
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
note_code_range(Section *s, long memaddr, bool discover)
{
  char *start = (char *) s->getSecAddr();
  char *end = start + s->getSecSize();
  string ntmp;
  new_code_range(start, end, memaddr, discover);

  add_function_entry(start, code_range_comment(ntmp, s->getSecName(), "start"), true /* global */);
  add_function_entry(end, code_range_comment(ntmp, s->getSecName(), "end"), true /* global */);
}


static void
note_section(Symtab *syms, const char *sname, bool discover)
{
  long memaddr = (long) syms->mem_image();
  Section *s;
  if (syms->findSection(s, sname) && s) 
    note_code_range(s, memaddr - syms->imageOffset(), discover);
}


static void
note_code_ranges(Symtab *syms, bool fn_discovery)
{
  note_section(syms, SECTION_INIT, fn_discovery);
  note_section(syms, SECTION_PLT, ALWAYS_DISCOVER_FUNCTIONS);
  note_section(syms, SECTION_TEXT, fn_discovery);
  note_section(syms, SECTION_FINI, fn_discovery);
}


static void 
dump_symbols(Symtab *syms, vector<Symbol *> &symvec, bool fn_discovery)
{
  note_code_ranges(syms, fn_discovery);

  //-----------------------------------------------------------------
  // collect function start addresses and pair them with a comment
  // that indicates what function (or functions) map to that start 
  // address. enter them into a data structure for reachable function
  // processing
  //-----------------------------------------------------------------
  for (int i = 0; i < symvec.size(); i++) {
    Symbol *s = symvec[i];
    Symbol::SymbolLinkage sl = s->getLinkage();
    if (report_symbol(s)) 
      add_function_entry((void *) s->getAddr(), &s->getName(), 
			 ((sl & Symbol::SL_GLOBAL) ||
			  (sl & Symbol::SL_WEAK)));
  }

  process_code_ranges();

  //-----------------------------------------------------------------
  // dump the address and comment for each function  
  //-----------------------------------------------------------------
  dump_reachable_functions();
}


static void 
dump_file_symbols(Symtab *syms, vector<Symbol *> &symvec, bool fn_discovery)
{
  if (c_fmt_fp() != NULL) {
    fprintf(c_fmt_fp(), "unsigned long csprof_nm_addrs[] = {\n");
  }

  dump_symbols(syms, symvec, fn_discovery);

  if (c_fmt_fp() != NULL) {
    fprintf(c_fmt_fp(), "};\nint csprof_nm_addrs_len = "
	   "sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);\n");
  }
}


// We call it "header", even though it comes at end of file.
//
static void
dump_header_info(int relocatable)
{
  struct fnbounds_file_header fh;

  if (binary_fmt_fd() >= 0) {
    memset(&fh, 0, sizeof(fh));
    fh.magic = FNBOUNDS_MAGIC;
    fh.num_entries = num_function_entries();
    fh.relocatable = relocatable;
    write(binary_fmt_fd(), &fh, sizeof(fh));
  }

  if (c_fmt_fp() != NULL) {
    fprintf(c_fmt_fp(), "unsigned int csprof_relocatable = %d;\n", relocatable);
    fprintf(c_fmt_fp(), "unsigned int csprof_stripped = %d;\n", 0);
  }

  if (text_fmt_fp() != NULL) {
    fprintf(text_fmt_fp(), "num symbols = %ld, relocatable = %d\n",
	    num_function_entries(), relocatable);
  }
}


static void 
dump_file_info(const char *filename, bool fn_discovery)
{
  Symtab *syms;
  string sfile(filename);
  vector<Symbol *> symvec;
  Symtab::openFile(syms, sfile);
  int relocatable = 0;

#ifdef USE_SYMTABAPI_EXCEPTION_BLOCKS 
  //-----------------------------------------------------------------
  // ensure that we don't infer function starts within try blocks or
  // at the start of catch blocks
  //-----------------------------------------------------------------
  vector<ExceptionBlock *> exvec;
  syms->getAllExceptions(exvec);
  for (int i = 0; i < exvec.size(); i++) {
    ExceptionBlock *e = exvec[i];

#ifdef DUMP_EXCEPTION_BLOCK_INFO
    printf("tryStart = %p tryEnd = %p, catchStart = %p\n", e->tryStart(), 
	   e->tryEnd(), e->catchStart()); 
#endif
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
#endif


  syms->getAllSymbolsByType(symvec, Symbol::ST_FUNCTION);
  if (syms->getObjectType() != obj_Unknown) {
    dump_file_symbols(syms, symvec, fn_discovery);
    relocatable = syms->isExec() ? 0 : 1;
  }
  dump_header_info(relocatable);
}


