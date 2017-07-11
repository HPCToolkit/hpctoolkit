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
// Copyright ((c)) 2002-2017, Rice University
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

//***************************************************************************
//
// File:
//   dso_symbols.cpp
//
// Purpose:
//   implementation that extracts function symbols from a shared library
//
//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>



//******************************************************************************
// dyninst includes
//******************************************************************************
#include <CodeObject.h>
#include <CodeSource.h>
#include <Function.h>
#include <Symtab.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/vdso.h>

#include "dso_symbols.h"



//******************************************************************************
// name spaces
//******************************************************************************

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;

using namespace std;



//******************************************************************************
// types
//******************************************************************************

typedef std::set<string> NameSet;

typedef std::map<Offset, NameSet *> FunctionEndpoints;

struct DsoInternalState {
  DsoInternalState
  (Symtab *_the_symtab, 
   dso_symbols_symbol_callback_t _note_symbol,
   void *_callback_arg,
   const char *_basename) :
    the_symtab(_the_symtab),
    note_symbol(_note_symbol),
    callback_arg(_callback_arg),
    basename(_basename) {};

  Symtab *the_symtab;
  dso_symbols_symbol_callback_t *note_symbol; 
  void *callback_arg;                        
  const char *basename;
};
  

//******************************************************************************
// global variables 
//******************************************************************************

FunctionEndpoints functionEndpoints;


//******************************************************************************
// private functions
//******************************************************************************


static const char *
fileBasename
(
 const char *pathname
)
{
  const char *slash = strrchr(pathname, '/');
  const char *basename = (slash ? slash + 1 : pathname);
  return basename;
}


#if 0
static int
dso_symbol_binding
(
 GElf_Sym *s
)
{
  switch(GELF_ST_BIND(s->st_info)){
  case STB_LOCAL:  return dso_symbol_bind_local;
  case STB_GLOBAL: return dso_symbol_bind_global;
  case STB_WEAK:   return dso_symbol_bind_weak;
  default:         return dso_symbol_bind_other;
  }
}
#endif


static void 
addFunction
(
 struct DsoInternalState *state,
 Offset offset,
 string name
)
{
  (*state->note_symbol)(name.c_str(),
			offset,
			dso_symbol_bind_global,
			state->callback_arg); 
}


static string
procSymbol
(
 const char *prefix,
 Offset addr,
 const char *basename
)
{
  std::ostringstream s;
  s << prefix 
    << "0x"
    << std::hex
    << addr
    << " [" << basename << "]"; 
  return s.str();
}


static string
unknownProcedure
(
 Offset addr,
 const char *basename
)
{
  return procSymbol("<unknown procedure>@", addr, basename);
}


static string
sectionEnd
(
 Offset addr,
 const char *basename
)
{
  return procSymbol("<end section>@", addr, basename);
}


static void
addRegion
(
 struct DsoInternalState *state,
 Region *s
)
{
  Offset start =  s->getDiskOffset();
  Offset end = start + s->getDiskSize();

  addFunction(state, start, unknownProcedure(start, state->basename));
  addFunction(state, end,   sectionEnd(end, state->basename));
}


static void 
addRegions
(
 struct DsoInternalState *state
)
{
  vector<Dyninst::SymtabAPI::Region *> regions;
  state->the_symtab->getCodeRegions(regions);
  for (unsigned int i = 0; i < regions.size(); i++) {
    Dyninst::SymtabAPI::Region *r = regions[i];
    addRegion(state, r); 
  }
}


static unsigned int 
addSymbols
(
 struct DsoInternalState *state,
 Symbol::SymbolType sType
) 
{
  vector<Dyninst::SymtabAPI::Symbol *> symbols;
  state->the_symtab->getAllSymbolsByType(symbols, sType);
  for (unsigned int i = 0; i < symbols.size(); i++) {
    Symbol *s = symbols[i];
    string mname = s->getMangledName();
    Offset o = s->getOffset();
    addFunction(state, o, mname);
  }
  return symbols.size();
}


static void 
addSymtabFunctions
(
 struct DsoInternalState *state
)
{
  vector<Dyninst::SymtabAPI::Function *> symtabFunctions;
  state->the_symtab->getAllFunctions(symtabFunctions);
  for (unsigned int i = 0; i < symtabFunctions.size(); i++) {
    Dyninst::SymtabAPI::Function *f = symtabFunctions[i];
    Offset o = f->getOffset();
    string names;
    for (auto n = f->mangled_names_begin(); n != f->mangled_names_end(); n++) {
      addFunction(state, o, *n);
    }
  }
}


static bool 
parseParseAPISyntheticName(const char *name, Offset &off)
{
  char buffer[2];
  intptr_t val;
  int success = sscanf(name, "targ%lx%1s", &val, buffer);
  if (success == 1) off = val;
  return success == 1;
}


static std::string 
functionName(std::string name, const char *basename)
{
  Offset off;
  bool isSynthetic = parseParseAPISyntheticName(name.c_str(), off);
  if (isSynthetic) return procSymbol("<unknown procedure>@", off, basename);
  else return name;
}


static void
addParseAPIfunctions
(
 struct DsoInternalState *state
)
{ 
  SymtabCodeSource *code_src = new SymtabCodeSource(state->the_symtab);
  CodeObject *the_code_obj = new CodeObject(code_src);

  the_code_obj->parse();

  const CodeObject::funclist &funcList = the_code_obj->funcs();
  for (auto fit = funcList.begin(); fit != funcList.end(); ++fit) {
    ParseAPI::Function * func = *fit;
    string fname = functionName(func->name(), state->basename);
    addFunction(state, func->addr(), fname);
  }
}


static int
dso_symbols_internal
(
 struct DsoInternalState *state
)
{
  int status_ok = 0;
  // state->the_symtab->parseTypesNow();
  
  addRegions(state);
  if (addSymbols(state, Symbol::ST_FUNCTION) == 0) {
    // no functions found; on Power [vdso] symbols are
    // ST_NOTYPE. sigh. rather than hard coding this as
    // a platform dependency, try this if "plan A" doesn't
    // yield any function symbols. 
    addSymbols(state, Symbol::ST_NOTYPE);
  }
  addSymtabFunctions(state);
  addParseAPIfunctions(state);
  
  status_ok = 1;
  return status_ok;
}



//******************************************************************************
// interface functions
//******************************************************************************

int
dso_symbols_vdso
(
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
)
{
  int status_ok = 0;
  const char *base_filename = VDSO_SEGMENT_NAME_SHORT;

  char *vdso_start = (char *) vdso_segment_addr();

  if (vdso_start) {
    Symtab *the_symtab;
    size_t vdso_len = vdso_segment_len();
    if (Symtab::openFile(the_symtab, vdso_start, vdso_len, base_filename)) {
      struct DsoInternalState state(the_symtab, note_symbol, callback_arg,
				    base_filename);
      status_ok = dso_symbols_internal(&state);
    }
  }

  return status_ok;
}


int
dso_symbols
(
 const char *filename,
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
)
{
  int status_ok = 0;
  Symtab *the_symtab;
  if (Symtab::openFile(the_symtab, filename)) {
      struct DsoInternalState state(the_symtab, note_symbol, callback_arg,
				    fileBasename(filename));
      status_ok = dso_symbols_internal(&state);
  }
  return status_ok;
}
