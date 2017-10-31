//******************************************************************************
// system include files
//******************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>



//******************************************************************************
// local include files
//******************************************************************************

#include <include/linux_info.h>
#include "KernelSymbols.hpp"



//******************************************************************************
// types
//******************************************************************************

class KernelSymbol {
public:
  KernelSymbol(uint64_t __addr, char __type, const char *__name); 
  std::string &name();
  uint64_t addr();
  void dump();

private:
  uint64_t _addr;
  char _type;
  std::string _name;
};


struct KernelSymbolsRepr {
  std::vector<KernelSymbol*> kernel_symbols; 
};



//******************************************************************************
// private operations
//******************************************************************************
KernelSymbol::KernelSymbol(
  uint64_t __addr, 
  char __type, 
  const char *__name) 
  : _addr(__addr), _type(__type), _name(__name) 
{
}
  
void  
KernelSymbol::dump() 
{
    std::cout << std::hex << _addr << std::dec 
	      << " " << _type << " " << _name << std::endl;
}


std::string &
KernelSymbol::name()
{
  return _name;
}


uint64_t
KernelSymbol::addr()
{
  return _addr;
}


static bool 
compare(KernelSymbol *s1, KernelSymbol *s2)
{
  return s1->addr() < s2->addr(); 
}



//******************************************************************************
// interface operations
//******************************************************************************

KernelSymbols::KernelSymbols() 
{
  R = new struct KernelSymbolsRepr;
}


bool
KernelSymbols::parseLinuxKernelSymbols()
{
  FILE *fp = fopen(LINUX_KERNEL_SYMBOL_FILE, "r");

  if (fp) {
    size_t len = 4096;
    char *line = (char *) malloc(len); 
    
    for(;;) {
      if (getline(&line, &len, fp) == EOF) break; // read a line from the file

      // parse the line into 3 or 4 parts
      char type;
      void *addr;
      char name[4096];
      char module[4096];
      module[0] = 0; // initialize module to null in case it is missing
      int result = sscanf(line, "%p %c %s %s\n", &addr, &type, name, module);

      if (result < 3) break;

      switch(type) {
      case 't':
      case 'T':
        // if module is non-empty, append it to name 
	if (strlen(module) > 0) { 
           strcat(name, " "); 
           strcat(name, module); 
        }
        // add name to the set of function symbols
	R->kernel_symbols.push_back(new KernelSymbol((uint64_t) addr, type, name));
      default:
	break;
      }
    }
    fclose(fp);
  }
  int size = R->kernel_symbols.size();

  if (size > 1) {
   std::sort (R->kernel_symbols.begin(), R->kernel_symbols.end(), compare);
  }

  return size > 0;
}


bool
KernelSymbols::find(uint64_t vma, std::string &fnname)
{
  int first = 0;
  int last = R->kernel_symbols.size() - 1;

  if (vma < R->kernel_symbols[first]->addr()) {
    return false;
  }

  if (vma >= R->kernel_symbols[last]->addr()) {
    fnname = R->kernel_symbols[last]->name();
    return true;
  }

  for(;;) { 
    int mid = (first + last + 1) >> 1;
    if (vma >= R->kernel_symbols[mid]->addr()) {
      first = mid;
    } else if (vma < R->kernel_symbols[mid]->addr()) {
      last = mid;
    } 
    if (last - first <= 1) {
      fnname = R->kernel_symbols[first]->name();
      return true;
    }
  }
}



void 
KernelSymbols::dump()
{
  for (auto it = R->kernel_symbols.begin(); it != R->kernel_symbols.end(); ++it) {
    (*it)->dump();
  }
}



//******************************************************************************
// unit test
//******************************************************************************
// #define UNIT_TEST

#ifdef UNIT_TEST

int main(int argc, char **argv)
{
  KernelSymbols syms;
  syms.parseLinuxKernelSymbols();
  syms.dump();

  uint64_t addr = 0;
  if (argc == 2) {
    sscanf(argv[1], "%p", &addr);
    std::string name;
    bool result = syms.find(addr, name);
    std::cout << "Lookup " << std::hex << "0x" << addr << std::dec 
	      << " (" << result << ")" << " --> " << name << std::endl; 
  }
}

#endif
