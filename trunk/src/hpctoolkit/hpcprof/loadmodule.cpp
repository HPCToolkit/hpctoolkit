// $Id$
// -*- C++ -*-

//***************************************************************************
//
// File:
//    loadmodule.cc
//
// Purpose:
//    LoadModule class for reading and gathering symbolic information
//    from a load module.  (based on GNU Binutils).
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (exec.cc).
//
//***************************************************************************

#ifdef __GNUC__
#pragma implementation
#endif

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "loadmodule.h"

//*************************** Forward Declarations **************************

using namespace std;

//*************************** Forward Declarations **************************

bool minisym_is_interesting(bfd *abfd, asymbol *p);

#define valueof(x) ((x)->section->vma + (x)->value)

int val_forward(const void *x, const void *y)
{
  asymbol *xp = *(asymbol **)x;
  asymbol *yp = *(asymbol **)y;
  long yv = valueof(yp);
  long xv = valueof(xp);
  if (xv < yv) return -1; 
  if (xv == yv) return 0; 
  return 1;
}

//***************************************************************************

/////////////////////////////////////////////////////////////////////////
// LoadModule

LoadModule::LoadModule()
{
  debug_ = 0;
  type_ = Unknown;
}

LoadModule::~LoadModule()
{
}

bool
LoadModule::line_info() const
{
  return true;
}

bool
LoadModule::file_info() const
{
  return true;
}

bool
LoadModule::func_info() const
{
  return true;
}

/////////////////////////////////////////////////////////////////////////
// BFDLoadModule

extern "C" { 
  char *cplus_demangle (const char *mangled, int options); 
  static int demangle_flags = (1 << 0) | (1 << 1);
  /* FIXME: DMGL_PARAMS | DMGL_ANSI */
}


int BFDLoadModule::bfdinitialized_ = 0;

BFDLoadModule::BFDLoadModule(const string &name)
{
  if (!bfdinitialized_) {
      bfd_init();
      bfdinitialized_ = 1;
    }
  bfd_ = 0;
  symbols_ = 0;

  read(name);
}

BFDLoadModule::~BFDLoadModule()
{
  cleanup();
}

void
BFDLoadModule::cleanup(bool clear_cache)
{
  name_ = "";
  if (bfd_) {
      bfd_close(bfd_);
      bfd_ = 0;
    }
  delete[] (char*)symbols_;
  symbols_ = 0;
  if (clear_cache) locmap_.clear();
}

extern "C" {
    static void
    BFDLoadModule_find_address_in_section(bfd*b, asection* sect, PTR exec)
    {
      BFDLoadModule::find_address_in_section(b, sect, exec);
    }
}

void
BFDLoadModule::read(const string &name)
{
  read_file(name);
}

bool BFDLoadModule::minisyms_;
int BFDLoadModule::dynoffset_;

void
BFDLoadModule::read_file(const string &name, bool clear_cache)
{
  cleanup(clear_cache);
  name_ = name;

#if 0
  cerr << "Reading: " << name << endl;
#endif
  bfd_ = bfd_openr(name_.c_str(), 0);
  minisyms_ = false;
  if (!bfd_) {
      cleanup();
      return;
    }

  if (!bfd_check_format(bfd_, bfd_object)) {
      cleanup();
      return;
    }

  flagword flags = bfd_get_file_flags(bfd_);
  if (flags & EXEC_P) {         // BFD is directly executable
    type_ = Exe;
  } else if (flags & DYNAMIC) { // BFD is a dynamic object
    type_ = DSO;
  } 

  if (type_ == DSO) {
    long symcount;
    PTR minisyms;
    unsigned int size;
    struct size_sym *symsizes;
    asymbol *tmp_symbol, *mk_sym;
    int i;
    
    symcount = bfd_read_minisymbols (bfd_, (bfd_boolean)true, &minisyms, &size);
    symbols_ = (asymbol**)(new char[(symcount+1)*sizeof(asymbol *)]);
#if 0
    cerr << "Mini symbols semiloaded: " << symcount << endl;
#endif
    tmp_symbol = NULL;
    asymbol *res = NULL;
    int nsyms = 0;
    for (i = 0; i < symcount; i++) {
      if (res == tmp_symbol) {
	tmp_symbol = bfd_make_empty_symbol (bfd_);
      }
      res = bfd_minisymbol_to_symbol(bfd_, (bfd_boolean)true, 
				     (PTR)(((char *)minisyms)+i*size), 
				     tmp_symbol);
      if (minisym_is_interesting(bfd_, res)) {
	symbols_[nsyms++] = res;
      } else {
	res = NULL;
      }
    }
    
    qsort(symbols_, nsyms, sizeof(asymbol*), val_forward);
    
    symbols_[nsyms] = NULL;
#if 0
    cerr << "Mini symbols loaded: " << nsyms << endl;
#endif
    minisyms_ = true;
  }
  else {
    long int symsize = bfd_get_symtab_upper_bound(bfd_);
    if (!symsize) {
      cleanup();
      return;
    }
    
    symbols_ = (asymbol**)(new char[symsize]);
    if (!symbols_) {
      cleanup();
      return;
    }
    
    int numsym = bfd_canonicalize_symtab(bfd_, symbols_);
#if 0
    cerr << "Number of symbols loaded: " << numsym << endl;
#endif
    
    minisyms_ = false;
  }
}

bfd_vma BFDLoadModule::pc_ = 0;
const char *BFDLoadModule::filename_ = 0;
const char *BFDLoadModule::functionname_ = 0;
unsigned int BFDLoadModule::line_ = 0;
bool BFDLoadModule::found_ = false;

bool 
minisym_is_interesting(bfd *abfd, asymbol *p)
{
   symbol_info syminfo;
   bfd_get_symbol_info (abfd, p, &syminfo);
   return (syminfo.type == 'T') || (syminfo.type == 't');
}

void
BFDLoadModule::find_address_in_section(bfd* abfd, asection* section, PTR exec)
{
  BFDLoadModule *e = static_cast<BFDLoadModule*>(exec);

  bfd_vma vma;
  bfd_size_type size;

  if (!e->debug() && e->found_)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0) {
    return;
  }

  vma = bfd_get_section_vma (abfd, section);
  size = bfd_get_section_size_before_reloc (section);

  // FIXME: these debugging messages should removed or placed within
  // debug conditionals
#if 0
  cerr << "Section: vma=" << vma << ", size=" << size << ", dynoffset=" 
       << dynoffset_ << ", flags=" << bfd_get_section_flags (abfd, section) 
       << endl;
#endif

  const char *tmp_filename = 0;
  const char *tmp_funcname = 0;
  unsigned int tmp_line = 0;
  bool tmp_found = false;

#if 0
  cerr << "Looking for pc_: " << e->pc_ << endl;
#endif
  if (minisyms_) {
    asymbol **p;
    unsigned long offset, bestoff;
    asymbol *bestsym;
    
    if (e->pc_ >= vma && e->pc_ < vma + size) {
      bfd_find_nearest_line (abfd, section, e->symbols_, e->pc_ - vma,
			     &tmp_filename, &tmp_funcname,
			     &tmp_line);
#if 0
      cerr << "FNL: pc=" << e->pc_ << ", funcname=" << tmp_funcname << endl;
#endif
      bestsym = (asymbol *)0;
      offset = e->pc_;
      bestoff = 0;
      for (p = e->symbols_; *p != NULL; p++) {
	symbol_info syminfo;
	bfd_get_symbol_info (abfd, *p, &syminfo);
#if 0
	cerr << "symbol: sym=" << syminfo.value << " offset=" << offset 
	     << " best=" << bestoff << " name=" << syminfo.name << endl;
#endif
	if (offset >= syminfo.value && bestoff < syminfo.value) {
	  bestoff = syminfo.value;
#if 0
	  cerr << "better symbol match: sym="<< offset 
	       << " best=" << bestoff << "name =" << syminfo.name << endl;
#endif
	  bestsym = (*p);
 	  tmp_funcname = syminfo.name;
	}
      }
      if (bestsym) {
        tmp_found = true;
        tmp_filename = (char *)0;
        tmp_line = 0;
#if 0
        cerr << "pc_: " << e->pc_ << ", vma: " << vma << ",size: " << size 
	     << ", bestoff: " << bestoff << ", funcname: " << tmp_funcname
	     << endl;
#endif
      }
    }
    dynoffset_ += size;
  }
  else {
    if (e->pc_ >= vma && e->pc_ < vma + size)
      tmp_found = bfd_find_nearest_line (abfd, section, e->symbols_, 
					 e->pc_ - vma, &tmp_filename, 
					 &tmp_funcname, &tmp_line);
  }
  if (!tmp_found) {
#if 0
    cerr << "pc_: " << (void*)e->pc_ << " NOT FOUND" << endl;
#endif
  }
  
  if (e->debug() && (tmp_found || tmp_filename || tmp_funcname || tmp_line)) {
    cerr << "    { bfd sec find: (" << tmp_found << ")"
	 << ", file = " << tmp_filename
	 << ", func = " << tmp_funcname
	 << ", line = " << tmp_line << " }" << endl;
  }
  if (!e->found_) {
    e->found_ = tmp_found;
    e->filename_ = tmp_filename;
    e->functionname_ = tmp_funcname;
    e->line_ = tmp_line;
  }
}

time_t
BFDLoadModule::mtime() const
{
  if (!bfd_) return 0;
  return bfd_get_mtime(bfd_);
}

std::string
BFDLoadModule::name() const
{
  return name_;
}

pprof_off_t
BFDLoadModule::textstart() const
{
  return 0;
}

string
BFDLoadModule::demangle(const string &name) const
{
 char *demangled = cplus_demangle(name.c_str(), demangle_flags);
 string r;
 if (demangled) {
     r = demangled;
     free(demangled);
   }
 else {
     r = name;
   }
 return r;
}

void
BFDLoadModule::find_bfd(pprof_off_t pc, const char **filename,
                        unsigned int *lineno, const char **funcname) const
{
  pc_ = pc;
  dynoffset_ = 0;
  found_ = false;
  functionname_ = 0;
  line_ = 0;
  filename_ = 0;
  bfd_map_over_sections(bfd_, BFDLoadModule_find_address_in_section,
                        (PTR) this);

  if (funcname) *funcname = functionname_;
  if (lineno) *lineno = line_;
  if (filename) *filename = filename_;
}

int
BFDLoadModule::find(pprof_off_t pc, const char **filename,
                    unsigned int *lineno, const char **funcname) const
{
  if (!bfd_) return 0;

  std::map<pprof_off_t,FuncInfo>::iterator ilocmap = locmap_.find(pc);
  if (ilocmap != locmap_.end()) {
      FuncInfo &funcinfo = (*ilocmap).second;
      if (funcinfo.file().size() > 0) *filename = funcinfo.c_file();
      else *filename = 0;
      if (funcinfo.name().size() > 0) *funcname = funcinfo.c_name();
      else *funcname = 0;
      *lineno = funcinfo.index();
      return 1;
    }

  find_bfd(pc, filename, lineno, funcname);

  if (line_ == 0) {
      // possible BFD problem--reinitialize and try again
      const_cast<BFDLoadModule*>(this)->read_file(name(),false);
      find_bfd(pc, filename, lineno, funcname);
      if (debug_ && line_ != 0) {
          cerr << "WARNING: BFD lineno == 0 problem detected" << endl;
        }
    }

  if (debug_) {
#if 0
      const char* pre = "    ";
      cerr << pre << "{ bfd find @ " << (void*)pc << ": " << *funcname << endl
	   << pre << "  [" << *filename << ":" << *lineno << "] }" << endl;
#endif
    }

  locmap_[pc].set_index(*lineno);
  if (*funcname) locmap_[pc].set_name(*funcname);
  if (*filename) locmap_[pc].set_file(*filename);

  return found_;
}

