
#ifdef __GNUC__
#pragma implementation
#endif

#include <iostream>

#include "exec.h"

using namespace std;

// forward declaration

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

/////////////////////////////////////////////////////////////////////////
// Executable

Executable::Executable()
{
  debug_ = 0;
  type_ = Unknown;
}

Executable::~Executable()
{
}

bool
Executable::line_info() const
{
  return true;
}

bool
Executable::file_info() const
{
  return true;
}

bool
Executable::func_info() const
{
  return true;
}

/////////////////////////////////////////////////////////////////////////
// BFDExecutable

extern "C" { char *cplus_demangle (const char *mangled, int options); }

int BFDExecutable::bfdinitialized_ = 0;

BFDExecutable::BFDExecutable(const string &name)
{
  if (!bfdinitialized_) {
      bfd_init();
      bfdinitialized_ = 1;
    }
  bfd_ = 0;
  symbols_ = 0;

  read(name);
}

BFDExecutable::~BFDExecutable()
{
  cleanup();
}

void
BFDExecutable::cleanup(bool clear_cache)
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
    BFDExecutable_find_address_in_section(bfd*b, asection* sect, PTR exec)
    {
      BFDExecutable::find_address_in_section(b, sect, exec);
    }
}

void
BFDExecutable::read(const string &name)
{
  read_file(name);
}

bool BFDExecutable::minisyms_;
int BFDExecutable::dynoffset_;

// FIXME: change to BFDLoadModule
void
BFDExecutable::read_file(const string &name, bool clear_cache)
{
  cleanup(clear_cache);
  name_ = name;

  cout << "Reading: " << name << endl;
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

      symcount = bfd_read_minisymbols (bfd_, 1, &minisyms, &size);
      symbols_ = (asymbol**)(new char[(symcount+1)*sizeof(asymbol *)]);
#if 1
      cout << "Mini symbols semiloaded: " << symcount << endl;
#endif
      tmp_symbol = NULL;
      asymbol *res = NULL;
      int nsyms = 0;
      for (i = 0; i < symcount; i++) {
        if (res == tmp_symbol) {
              tmp_symbol = bfd_make_empty_symbol (bfd_);
	}
	res = bfd_minisymbol_to_symbol(bfd_, 1, 
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
      cout << "Mini symbols loaded: " << nsyms << endl;
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
    cout << "Number of symbols loaded: " << numsym << endl;

      minisyms_ = false;
  }
}

bfd_vma BFDExecutable::pc_ = 0;
const char *BFDExecutable::filename_ = 0;
const char *BFDExecutable::functionname_ = 0;
unsigned int BFDExecutable::line_ = 0;
bool BFDExecutable::found_ = false;

bool minisym_is_interesting(bfd *abfd, asymbol *p)
{
   symbol_info syminfo;
   bfd_get_symbol_info (abfd, p, &syminfo);
   return (syminfo.type == 'T') || (syminfo.type == 't');
}

void
BFDExecutable::find_address_in_section(bfd*abfd, asection* section, PTR exec)
{
  BFDExecutable *e = static_cast<BFDExecutable*>(exec);

  bfd_vma vma;
  bfd_size_type size;

  if (!e->debug() && e->found_)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0){
    return;
  }

  vma = bfd_get_section_vma (abfd, section);
  size = bfd_get_section_size_before_reloc (section);

  cout << "Section: vma=" << vma << ", size=" << size <<
	  ", dynoffset=" << dynoffset_ <<
          ", flags=" << bfd_get_section_flags (abfd, section) <<
	  endl;

  const char *tmp_filename = 0;
  const char *tmp_funcname = 0;
  unsigned int tmp_line = 0;
  bool tmp_found = false;

  cout << "Looking for pc_: " << e->pc_ << endl;
  if (minisyms_) {
     asymbol **p;
     unsigned long offset, bestoff;
     asymbol *bestsym;

     if (e->pc_ >= vma && e->pc_ < vma + size) {
      bfd_find_nearest_line (abfd, section, e->symbols_, e->pc_ - vma,
				     &tmp_filename, &tmp_funcname,
				     &tmp_line);
     cout << "FNL: pc=" << e->pc_ << ", funcname=" << tmp_funcname << endl;
     bestsym = (asymbol *)0;
     offset = e->pc_;
     bestoff = 0;
     for (p = e->symbols_; *p != NULL; p++) {
	symbol_info syminfo;
	bfd_get_symbol_info (abfd, *p, &syminfo);
	cout << "symbol: sym=" << syminfo.value << " offset=" << offset << " best=" << bestoff << " name=" << syminfo.name << endl;
	if (offset >= syminfo.value && bestoff < syminfo.value) {
	  bestoff = syminfo.value;
#if 0
	  cout << "better symbol match: sym="<< offset << " best=" << bestoff << "name =" << syminfo.name << endl;
#endif
	  bestsym = (*p);
 	  tmp_funcname = syminfo.name;
	}
      }
      if (bestsym) {
        tmp_found = true;
        tmp_filename = (char *)0;
        tmp_line = 0;
        cout << "pc_: " << e->pc_ << ", vma: " << vma << ",size: " << size << ", bestoff: " << bestoff << ", funcname: " << tmp_funcname << endl;
      }
    }
    dynoffset_ += size;
  }
  else {
      if (e->pc_ >= vma && e->pc_ < vma + size)

      tmp_found = bfd_find_nearest_line (abfd, section, e->symbols_, e->pc_ - vma,
				     &tmp_filename, &tmp_funcname,
				     &tmp_line);
  }
  if (!tmp_found)
        cout << "pc_: " << e->pc_ << " NOT FOUND" << endl;

  if (e->debug() && (tmp_found || tmp_filename || tmp_funcname || tmp_line)) {
#if 0
      cout << "    { bfd sec find: (" << tmp_found << ")"
           << ", file = " << tmp_filename
           << ", func = " << tmp_funcname
           << ", line = " << tmp_line << " }" << endl;
#endif
    }
  if (!e->found_) {
      e->found_ = tmp_found;
      e->filename_ = tmp_filename;
      e->functionname_ = tmp_funcname;
      e->line_ = tmp_line;
    }
}

time_t
BFDExecutable::mtime() const
{
  if (!bfd_) return 0;
  return bfd_get_mtime(bfd_);
}

std::string
BFDExecutable::name() const
{
  return name_;
}

vmon_off_t
BFDExecutable::textstart() const
{
  return 0;
}

string
BFDExecutable::demangle(const string &name) const
{
 char *demangled = cplus_demangle(name.c_str(), 1);
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
BFDExecutable::find_bfd(vmon_off_t pc, const char **filename,
                        unsigned int *lineno, const char **funcname) const
{
  pc_ = pc;
  dynoffset_ = 0;
  found_ = false;
  functionname_ = 0;
  line_ = 0;
  filename_ = 0;
  bfd_map_over_sections(bfd_, BFDExecutable_find_address_in_section,
                        (PTR) this);

  if (funcname) *funcname = functionname_;
  if (lineno) *lineno = line_;
  if (filename) *filename = filename_;
}

int
BFDExecutable::find(vmon_off_t pc, const char **filename,
                    unsigned int *lineno, const char **funcname) const
{
  if (!bfd_) return 0;

  std::map<vmon_off_t,FuncInfo>::iterator ilocmap = locmap_.find(pc);
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
      const_cast<BFDExecutable*>(this)->read_file(name(),false);
      find_bfd(pc, filename, lineno, funcname);
      if (debug_ && line_ != 0) {
          cout << "WARNING: BFD lineno == 0 problem detected" << endl;
        }
    }

  if (debug_) {
#if 0
      const char* pre = "    ";
      cout << pre << "{ bfd find @ " << (void*)pc << ": " << *funcname << endl
	   << pre << "  [" << *filename << ":" << *lineno << "] }" << endl;
#endif
    }

  locmap_[pc].set_index(*lineno);
  if (*funcname) locmap_[pc].set_name(*funcname);
  if (*filename) locmap_[pc].set_file(*filename);

  return found_;
}


/////////////////////////////////////////////////////////////////////////
// COFFExecutable

#if 0 /* FIXME: throw this stuff away */
//#ifdef HAVE_LIBLD

#ifndef COFF_DEBUG
//#define COFF_DEBUG
#endif

COFFExecutable::COFFExecutable(const string &name)
{
  ldfile_ = 0;
  read(name);
}

COFFExecutable::~COFFExecutable()
{
  cleanup();
}

void
COFFExecutable::cleanup()
{
  name_ = "";
  textstart_ = 0;
  locmap_.clear();

  if (ldfile_) ldclose(ldfile_);
}

void
COFFExecutable::read(const string &name)
{
  cleanup();
  name_ = name;
  textstart_ = 0;
  is64_ = false;

  ldfile_ = ldopen(const_cast<char *>(name.c_str()),0);
  if (HEADER(ldfile_).f_magic == U802TOCMAGIC
      || HEADER(ldfile_).f_magic == U803XTOCMAGIC) {

      if (HEADER(ldfile_).f_magic == U803XTOCMAGIC) is64_ = true;

      if (is64()) init64();
      else init32();
    }
  else {
      cleanup();
    }
}

time_t
COFFExecutable::mtime() const
{
  if (!ldfile_) return 0;
  return HEADER(ldfile_).f_timdat;
}

std::string
COFFExecutable::name() const
{
  return name_;
}

unsigned int
COFFExecutable::textstart() const
{
  return 0;
}

string
COFFExecutable::demangle(const string &name) const
{
 return name;
}

void
COFFExecutable::init32()
{
  void *ventry = new char[SYMESZ];
  SYMENT *entry = static_cast<SYMENT*>(ventry);
  void *vauxentry = new char[AUXESZ];
  AUXENT *auxentry = static_cast<AUXENT*>(vauxentry);
  int entry_number = 0;
  string lastfile;
  while (ldtbread(ldfile_, entry_number, entry) == SUCCESS) {
      string name = ldgetname(ldfile_, entry);
#ifdef COFF_DEBUG
      cout << "entry " << entry_number
           << " name = " << name
           << " isfcn = " << ISFCN(entry->n_type)
           << " value = " << (void*) entry->n_value
           << " naux = " << (int) entry->n_numaux
           << " type = " << (int) entry->n_type
           << " sclass = " << (int) entry->n_sclass
           << endl;
#endif
      if (ISFCN(entry->n_type)) {
          locmap_[entry->n_value].set_name(name);
          locmap_[entry->n_value].set_file(lastfile);
          locmap_[entry->n_value].set_index(entry_number);
#ifdef COFF_DEBUG
          cout << "added " << name
               << " at " << (void*) entry->n_value
               << endl;
          for (int j=0; j<4; j++) {
              cout << " i" << j << " = " << ((int*)entry)[j]
                   << " s" << j*2 << " = " << ((short*)entry)[j*2]
                   << " s" << j*2+1 << " = " << ((short*)entry)[j*2+1]
                   << endl;
            }
          for (int i=1; i<=entry->n_numaux; i++) {
              ldtbread(ldfile_, entry_number+i, auxentry);
              cout << "fnc aux " << i
                   << " lnno = " << auxentry->x_sym.x_misc.x_lnsz.x_lnno
                   << " fncary aux lnnoptr = "
                   << auxentry->x_sym.x_fcnary.x_fcn.x_lnnoptr
                   << endl;
              for (int j=0; j<4; j++) {
                  cout << " i" << j << " = " << ((int*)auxentry)[j]
                       << " s" << j*2 << " = " << ((short*)auxentry)[j*2]
                       << " s" << j*2+1 << " = " << ((short*)auxentry)[j*2+1]
                       << endl;
                }
            }
#endif
        }
      if (name == ".file") {
          lastfile = "";
          for (int i=1; i<=entry->n_numaux; i++) {
              ldtbread(ldfile_, entry_number+i, auxentry);
              if (auxentry->x_file._x.x_ftype == XFT_FN)
                  lastfile = ldgetname(ldfile_, auxentry);
            }
        }
#ifdef COFF_DEBUG
      for (int i=1; i<=entry->n_numaux; i++) {
          ldtbread(ldfile_, entry_number+i, auxentry);
          cout << " aux " << i;
          cout << " name = " << ldgetname(ldfile_, auxentry);
          cout << " size = " << auxentry->x_sym.x_misc.x_fsize;
          cout << endl;
        }
#endif
      entry_number += 1 + entry->n_numaux;
    }

  delete[] (char*) ventry;
  delete[] (char*) vauxentry;
}

void
COFFExecutable::init64()
{
  cout << "ERROR: cannot use 64 bit symtab" << endl;
  abort();
}

int
COFFExecutable::find(vmon_off_t pc, const char **filename,
                 unsigned int *lineno, const char **funcname) const
{
  *filename = 0;
  *lineno = 0;
  *funcname = 0;
  if (is64()) {
      return find64(pc, filename, lineno, funcname);
    }
  return find32(pc, filename, lineno, funcname);
}

int
COFFExecutable::find32(vmon_off_t pc, const char **filename,
                      unsigned int *lineno, const char **funcname) const
{
  // sets li to one past the function of interest
  map<uint64_t, FuncInfo>::const_iterator li = locmap_.upper_bound(pc);
  // cannot decrement the first item
  if (li == locmap_.begin()) return 0;
  li--;
  // can happen for an empty set
  if (li == locmap_.end()) return 0;
  *funcname = (*li).second.c_name();
  *filename = (*li).second.c_file();
  int index = (*li).second.index();

  void *vline = new char[LINESZ];
  LINENO *line = static_cast<LINENO*>(vline);

  // try finding the line number
#ifdef COFF_DEBUG
  cout << "looking for line info for index = " << index << endl;
#endif
  if (ldlinit(ldfile_, index) == SUCCESS) {
      int i=1;
      while (ldlitem(ldfile_,i,line) == SUCCESS) {
#ifdef COFF_DEBUG
          cout << "line " << i
               << " paddr = " << (void*)line->l_addr.l_paddr
               << " lnno = " << line->l_lnno
               << endl;
#endif
          if (pc > line->l_addr.l_paddr) *lineno = line->l_lnno;
          i++;
        }
    }
  else {
#ifdef COFF_DEBUG
      cout << "ldlinit failed" << endl;
#endif
    }

  delete[] (char*) vline;

  return 1;
}

int
COFFExecutable::find64(vmon_off_t pc, const char **filename,
                      unsigned int *lineno, const char **funcname) const
{
  return 0;
}

bool
COFFExecutable::line_info() const
{
  return false;
}
//#endif
#endif
