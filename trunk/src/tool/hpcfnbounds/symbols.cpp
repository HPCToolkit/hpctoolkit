//
// $Id$
//

#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include "code-ranges.h"
#include "process-ranges.h"
#include "function-entries.h"
#include "fnbounds-file-header.h"
#include "Symtab.h"
#include "Symbol.h"

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;

#define BINARY_FORMAT_ARG  "-b"  // write output in binary format
#define NO_DISCOVERY_ARG   "-d"  // don't perform function discovery
#define HELP_ARG           "-h"

#define SECTION_SYMTAB ".symtab"
#define SECTION_INIT   ".init"
#define SECTION_FINI   ".fini"
#define SECTION_TEXT   ".text"
#define SECTION_PLT    ".plt"

#define PATHSCALE_EXCEPTION_HANDLER_PREFIX "Handler."
#define STRLEN(s) (sizeof(s) - 1)

static void usage(char *command);
static void dump_file_info(const char *filename, bool fn_discovery);

extern "C" {

// we don't care about demangled names. define
// a no-op that meets symtabAPI semantic needs only
char *
cplus_demangle(char *s, int opts)
{
return strdup(s);
}  

};

static int use_binary_format = 0;


//*****************************************************************
// interface operations
//*****************************************************************

int 
main(int argc, char **argv)
{
  bool fn_discovery = 1;
  int n;

  for (n = 1; n < argc; n++) {
    if (strcmp(argv[n], BINARY_FORMAT_ARG) == 0)
      use_binary_format = 1;
    else if (strcmp(argv[n], NO_DISCOVERY_ARG) == 0)
      fn_discovery = 0;
    else if (strcmp(argv[n], HELP_ARG) == 0)
      usage(argv[0]);
    else
      break;
  }

  if (n >= argc)
    usage(argv[0]);

  dump_file_info(argv[n], fn_discovery);

  return 0;
}

int
binary_format(void)
{
  return (use_binary_format);
}


//*****************************************************************
// private operations
//*****************************************************************

static bool 
file_is_stripped(Symtab *syms)
{
  Section *sec;
  if (syms->findSection(sec, SECTION_SYMTAB)  == false) return true;
  return bool(sec == NULL);
}


static void 
usage(char *command)
{
  fprintf(stderr, 
	  "Usage: %s [%s] [%s] [%s] object-file\n"
	  "\t%s\twrite output in binary format\n"
	  "\t%s\tdon't perform function discovery on stripped code\n"
	  "\t%s\tprint this help message and exit\n",
	  command, BINARY_FORMAT_ARG, NO_DISCOVERY_ARG, HELP_ARG,
	  BINARY_FORMAT_ARG, NO_DISCOVERY_ARG, HELP_ARG);

  exit(0);
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
#if 1
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
  if (! binary_format()) {
    printf("unsigned long csprof_nm_addrs[] = {\n");
  }

  dump_symbols(syms, symvec, fn_discovery);

  if (! binary_format()) {
    printf("};\nint csprof_nm_addrs_len = "
	   "sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);\n");
  }
}


static void
dump_header_info(int relocatable, int stripped)
{
  struct fnbounds_file_header fh;

  if (binary_format()) {
    memset(&fh, 0, sizeof(fh));
    fh.magic = FNBOUNDS_MAGIC;
    fh.num_entries = num_function_entries();
    fh.relocatable = relocatable;
    fh.stripped = stripped;
    write(1, &fh, sizeof(fh));
  } else {
    printf("unsigned int csprof_relocatable = %d;\n", relocatable);
    printf("unsigned int csprof_stripped = %d;\n", stripped);
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
  int stripped = 0;


#if 0
  // SymtabAPI isn't yet providing reliable ExceptionBlock information for 
  // binaries created with PGI, Pathscale, and g++ compilers on x85_64
  //
  // ensure that we don't infer function starts within try blocks or
  // at the start of catch blocks
  vector<ExceptionBlock *> exvec;
  syms->getAllExceptions(exvec);
  for (int i = 0; i < exvec.size(); i++) {
    ExceptionBlock *e = exvec[i];

#if 0
    printf("tryStart = %p tryEnd = %p, catchStart = %p\n", e->tryStart(), 
	   e->tryEnd(), e->catchStart()); 
#endif
    add_protected_range((void *) e->tryStart(), (void *) e->tryEnd());
    
    long cs = e->catchStart(); 
    add_protected_range((void *) cs, (void *) (cs + 1));
  }
#endif


  syms->getAllSymbolsByType(symvec, Symbol::ST_FUNCTION);
  if (syms->getObjectType() != obj_Unknown) {
    dump_file_symbols(syms, symvec, fn_discovery);
    relocatable = syms->isExec() ? 0 : 1;
  }
  dump_header_info(relocatable, stripped);
}
