#include <iostream>
#include <vector>
#include <string>
#include <stdio.h>

#include "code-ranges.h"
#include "process-ranges.h"
#include "Symtab.h"
#include "Symbol.h"

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;

#define NO_DISCOVERY_ARGUMENT "-d" // don't perform function discovery

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



//*****************************************************************
// interface operations
//*****************************************************************

int 
main(int argc, char **argv)
{
  bool fn_discovery = 1;
  if (argc < 2) usage(argv[0]);

  const char *file = argv[--argc];

  if (argc == 2) {
    const char *arg = argv[--argc];
    if (strcmp(arg, NO_DISCOVERY_ARGUMENT) == 0) fn_discovery = 0;
    else usage(argv[0]);
  }
    
  dump_file_info(file, fn_discovery);
  return 0;
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
	 "Usage: %s [%s] object-file\n"
	 "\t%s\tdon't perform function discovery on stripped code\n",
	 command, NO_DISCOVERY_ARGUMENT, NO_DISCOVERY_ARGUMENT);
  
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
  if (pathscale_filter(sym)) return false;
  return true;
}


static void
note_code_range(Section *s, long memaddr)
{
  char *start = (char *) s->getSecAddr();
  char *end = start + s->getSecSize();
  new_code_range(start, end, memaddr);
      
}


static void
note_section(Symtab *syms, const char *sname)
{
  long memaddr = (long) syms->mem_image();
  Section *s;
  if (syms->findSection(s, sname) && s) note_code_range(s, memaddr - syms->imageOffset());
}


static void
note_code_ranges(Symtab *syms)
{
  note_section(syms, SECTION_INIT);
  note_section(syms, SECTION_FINI);
  note_section(syms, SECTION_TEXT);
  note_section(syms, SECTION_PLT);
}


static int 
dump_symbols(Symtab *syms, vector<Symbol *> &symvec, int fn_discovery)
{
  string comment;
  void *last = 0;
  void *first = 0;

  //-----------------------------------------------------------------
  // collect function start addresses and pair them with a comment
  // that indicates what function (or functions) map to that start 
  // address. enter them into a data structure for reachable function
  // processing
  //-----------------------------------------------------------------
  for (int i = 0; i < symvec.size(); i++) {
    Symbol *s = symvec[i];
    if (report_symbol(s)) {
      void *addr = (void *) s->getAddr();
      if (first == 0) first = addr;
      if (last == addr) { 
	comment  = comment + ", " + s->getName();
      } else {
	if (last) {
	  comment += " */";
	  new_function_entry(last, new string(comment));
	}
        comment = "/* " + s->getName();
	last = addr;
      }
    }
  }
  if (last) { 
    comment += " */";
    new_function_entry(last, new string(comment));
  }
  
  note_code_ranges(syms);
  process_code_ranges(fn_discovery);

  //-----------------------------------------------------------------
  // dump the address and comment for each function  
  //-----------------------------------------------------------------
  dump_reachable_functions();
  return (first == 0);
}


static int 
dump_file_symbols(Symtab *syms, vector<Symbol *> &symvec, bool fn_discovery)
{
  int no_symbols_dumped;
  printf("unsigned long csprof_nm_addrs[] = {\n");

  no_symbols_dumped = dump_symbols(syms, symvec, fn_discovery);

  printf("};\nint csprof_nm_addrs_len = "
	 "sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);\n");
  return no_symbols_dumped;
}


static void 
dump_file_info(const char *filename, bool fn_discovery)
{
  Symtab *syms;
  string sfile(filename);
  vector<Symbol *> symvec;
  Symtab::openFile(syms, sfile);
  int stripped = 1;
  bool is_stripped = file_is_stripped(syms);

  syms->getAllSymbolsByType(symvec, Symbol::ST_FUNCTION);
  if (syms->getObjectType() != obj_Unknown) {
    if (symvec.size() > 0) {
      // be conservative with function discovery: only apply it to stripped 
      // objects
      stripped = dump_file_symbols(syms, symvec, fn_discovery & is_stripped);
    }
    int relocatable = syms->isExec() ? 0 : 1;
    printf("unsigned int csprof_relocatable = %d;\n", relocatable);
  }
  printf("unsigned int csprof_stripped = %d;\n", stripped);
}
