#include <iostream>
#include <vector>
#include <string>
#include <stdio.h>

using namespace std;

#include "Symtab.h"
#include "Symbol.h"

using namespace Dyninst;
using namespace SymtabAPI;

#define PATHSCALE_EXCEPTION_HANDLER_PREFIX "Handler."
#define STRLEN(s) (sizeof(s) - 1)

static void dump_file_info(const char *filename);

extern "C" {
   // we don't care about demangled names. define
   // a no-op that meets symtabAPI semantic needs
   // only
   char *cplus_demangle(char *s, int opts)
   {
     return strdup(s);
   }  
};

int 
main(int argc, char **argv)
{
  dump_file_info(argv[1]);
  return 0;
}

static bool matches_prefix(string s, const char *pre, int n)
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
dump_file_symbols(vector<Symbol *> &symvec)
{
  string comment;
  printf("unsigned long csprof_nm_addrs[] = {\n");
  void *last = 0;
  for (int i = 0; i < symvec.size(); i++) {
    Symbol *s = symvec[i];
    if (report_symbol(s)) {
      void *addr = (void *) s->getAddr();
      if (last == addr) { 
	comment  = comment + ", " + s->getName();
      } else {
	if (last) {
          printf(", %s */\n", comment.c_str());
	}
        printf("  0x%lx", addr);
        comment = "/* " + s->getName();
	last = addr;
      }
    }
  }
  if (last)  printf(", %s */\n", comment.c_str());
  printf("\n};\nint csprof_nm_addrs_len = "
	 "sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);\n");
}


static void 
dump_file_info(const char *filename)
{
  Symtab *syms;
  string sfile(filename);
  vector<Symbol *> symvec;
  Symtab::openFile(syms, sfile);
  syms->getAllSymbolsByType(symvec, Symbol::ST_FUNCTION);
  int stripped = 1;
  if (syms->getObjectType() != obj_Unknown) {
    if (symvec.size() > 0) {
      stripped = 0;
      dump_file_symbols(symvec);
    }
    int relocatable = syms->isExec() ? 0 : 1;
    printf("unsigned int csprof_relocatable = %d;\n",relocatable);
  }
  printf("unsigned int csprof_stripped = %d;\n",stripped);
}
